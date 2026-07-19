'use strict';
/*
 * ChurchCam device simulator.
 *
 *   - 4 virtual PTZ cameras (2 VISCA, 2 ONVIF) that receive the real commands
 *     the controller sends and report pan/tilt/zoom to a browser that renders
 *     them as live, panning views.
 *   - A virtual ATEM Mini switcher (Blackmagic protocol) so the controller can
 *     select which camera is Program / Preview from on-screen buttons.
 *
 * Zero dependencies -- pure Node built-ins (http, net, dgram). Run: node server.js
 *
 * Point your ChurchCam config at this machine's IP:
 *   ATEM Switcher IP : <this machine>
 *   Camera 1         : VISCA  UDP  port 52381   (ATEM input 1 -> view 1)
 *   Camera 2         : VISCA  TCP  port 52382   (ATEM input 2 -> view 2)
 *   Camera 3         : ONVIF  TCP  port 8083    (ATEM input 3 -> view 3)
 *   Camera 4         : ONVIF  TCP  port 8084    (ATEM input 4 -> view 4)
 * Then open http://localhost:8099
 */

const http = require('http');
const net = require('net');
const dgram = require('dgram');
const fs = require('fs');
const path = require('path');
const os = require('os');

const WEB_PORT = Number(process.env.PORT) || 8099;
const ATEM_PORT = 9910;
const BOOT_ID = Date.now(); // page auto-reloads when this changes (server restarted)
const WSD_PORT = 3702; // ONVIF WS-Discovery multicast port

// This machine's LAN IP -- used in ONVIF XAddrs so the controller knows where
// to reach each camera (same IP, different ports).
const LAN_IP = (() => {
  for (const addrs of Object.values(os.networkInterfaces()))
    for (const a of addrs) if (a.family === 'IPv4' && !a.internal) return a.address;
  return '127.0.0.1';
})();

// View index -> transport. ATEM input (index+1) selects the view.
const VIEWS = [
  { id: 0, name: 'Camera 1', proto: 'VISCA', transport: 'udp', port: 52381, framed: true },
  { id: 1, name: 'Camera 2', proto: 'VISCA', transport: 'tcp', port: 52382, framed: false },
  { id: 2, name: 'Camera 3', proto: 'ONVIF', transport: 'http', port: 8083 },
  { id: 3, name: 'Camera 4', proto: 'ONVIF', transport: 'http', port: 8084 },
];

// ---------------------------------------------------------------------------
// SSE fan-out to browsers
// ---------------------------------------------------------------------------
const sseClients = new Set();
function broadcast(evt) {
  const line = `data: ${JSON.stringify(evt)}\n\n`;
  for (const res of sseClients) {
    try { res.write(line); } catch (_) { /* client gone */ }
  }
}
function logCmd(view, msg) {
  const tag = view != null ? `cam${view + 1}` : 'atem';
  const ts = new Date().toISOString().slice(11, 23); // HH:MM:SS.mmm
  console.log(`${ts} [${tag}] ${msg}`);
}

// ---------------------------------------------------------------------------
// Web server: static page, SSE stream, ATEM button POSTs
// ---------------------------------------------------------------------------
const web = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://x');

  if (req.method === 'GET' && url.pathname === '/events') {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      Connection: 'keep-alive',
    });
    res.write('\n');
    sseClients.add(res);
    // Boot id first (page reloads if it changed), then current ATEM state.
    res.write(`data: ${JSON.stringify({ kind: 'hello', boot: BOOT_ID })}\n\n`);
    res.write(`data: ${JSON.stringify({ kind: 'atem', program: atem.program, preview: atem.preview })}\n\n`);
    req.on('close', () => sseClients.delete(res));
    return;
  }

  if (req.method === 'POST' && (url.pathname === '/atem/program' || url.pathname === '/atem/preview')) {
    let body = '';
    req.on('data', (c) => (body += c));
    req.on('end', () => {
      let input = 1;
      try { input = Number(JSON.parse(body || '{}').input) || 1; } catch (_) {}
      input = Math.max(1, Math.min(VIEWS.length, input));
      if (url.pathname === '/atem/program') atem.setProgram(input);
      else atem.setPreview(input);
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ program: atem.program, preview: atem.preview }));
    });
    return;
  }

  // Static files from ./public
  let p = url.pathname === '/' ? '/index.html' : url.pathname;
  const file = path.join(__dirname, 'public', path.normalize(p).replace(/^(\.\.[/\\])+/, ''));
  fs.readFile(file, (err, data) => {
    if (err) { res.writeHead(404); res.end('Not found'); return; }
    const ext = path.extname(file);
    const type = ext === '.html' ? 'text/html' : ext === '.js' ? 'text/javascript' : 'text/plain';
    res.writeHead(200, { 'Content-Type': type });
    res.end(data);
  });
});
web.listen(WEB_PORT, () => console.log(`web  : http://localhost:${WEB_PORT}`));

// ---------------------------------------------------------------------------
// VISCA parsing (shared by UDP and TCP)
// ---------------------------------------------------------------------------
// Direction bytes (from Visca.cpp ptzDrive): dir1 1=left 2=right 3=none,
// dir2 1=up 2=down 3=none. Speeds are 0..0x18.
// A VISCA discovery reply block (see the reply layout in Visca.cpp). The
// controller keys on the source ip:port and parses NAME:.
function viscaDiscoveryReply(view) {
  return Buffer.concat([
    Buffer.from([0x02]),
    Buffer.from(`MODEL:SIM-VISCA`), Buffer.from([0xff]),
    Buffer.from(`IPADR:${LAN_IP}`), Buffer.from([0xff]),
    Buffer.from(`NAME:Camera ${view.id + 1}`), Buffer.from([0xff]),
    Buffer.from([0x03]),
  ]);
}

function parseVisca(buf, view, reply) {
  // VISCA discovery probe ("ENQ:network") -> reply with a device block
  if (buf.includes('ENQ')) {
    logCmd(view.id, 'VISCA discovery probe -> reply');
    if (reply) reply(viscaDiscoveryReply(view));
    return;
  }
  // Strip the 8-byte VISCA-over-IP header if present (UDP/framed sends it).
  let seq = 0;
  let p = buf;
  if (p.length >= 8 && p[0] === 0x01 && p[1] === 0x00) {
    seq = (p[4] << 24) | (p[5] << 16) | (p[6] << 8) | p[7];
    p = p.subarray(8);
  }
  if (p.length < 5 || p[0] !== 0x81) return; // not a VISCA payload we know

  // Power inquiry: 81 09 04 00 FF  -> reply power ON
  if (p[1] === 0x09 && p[2] === 0x04 && p[3] === 0x00) {
    logCmd(view.id, 'VISCA power inquiry -> ON');
    if (reply) replyViscaPowerOn(view, seq, reply);
    broadcast({ kind: 'ping', cam: view.id });
    return;
  }
  if (p[1] !== 0x01) return;

  // Pan/Tilt drive: 81 01 06 01 [pan] [tilt] [dir1] [dir2] FF
  if (p[2] === 0x06 && p[3] === 0x01 && p.length >= 8) {
    const panSpd = p[4], tiltSpd = p[5], dir1 = p[6], dir2 = p[7];
    const panDir = dir1 === 1 ? -1 : dir1 === 2 ? 1 : 0;
    const tiltDir = dir2 === 1 ? 1 : dir2 === 2 ? -1 : 0; // up = +
    const pan = panDir * Math.min(panSpd, 24) / 24;
    const tilt = tiltDir * Math.min(tiltSpd, 24) / 24;
    logCmd(view.id, `VISCA PT drive pan=${pan.toFixed(2)} tilt=${tilt.toFixed(2)}`);
    broadcast({ kind: 'pt', cam: view.id, pan, tilt });
    return;
  }
  // Zoom: 81 01 04 07 [dir] FF   dir 2=tele(in) 3=wide(out) 0=stop
  if (p[2] === 0x04 && p[3] === 0x07) {
    const d = p[4] & 0x0f;
    const zoom = d === 2 ? 1 : d === 3 ? -1 : 0;
    logCmd(view.id, `VISCA zoom ${zoom > 0 ? 'tele' : zoom < 0 ? 'wide' : 'stop'}`);
    broadcast({ kind: 'zoom', cam: view.id, zoom });
    return;
  }
  // Memory: 81 01 04 3F [01 set|02 recall] [n] FF
  if (p[2] === 0x04 && p[3] === 0x3f && p.length >= 6) {
    const action = p[4] === 0x01 ? 'set' : 'goto';
    const n = p[5];
    logCmd(view.id, `VISCA preset ${action} ${n}`);
    broadcast({ kind: 'preset', cam: view.id, action, n });
    return;
  }
}

// VISCA power INQUIRY reply. A real camera answers an inquiry with the completion
// message *directly* -- no ACK (ACKs are only for control commands that take time
// to execute). Sending a separate ACK here is both unfaithful and breaks the
// controller over TCP: the ACK and COMPLETE coalesce into one segment, and the
// controller's fixed-offset power-byte read then lands on the ACK's terminator.
// Framed for UDP, raw for TCP -- matching the controller's read.
function replyViscaPowerOn(view, seq, reply) {
  const frame = (payload) => {
    if (!view.framed) return Buffer.from(payload);
    const b = Buffer.alloc(8 + payload.length);
    b[0] = 0x01; b[1] = 0x11;                 // reply type
    b.writeUInt16BE(payload.length, 2);
    b.writeUInt32BE(seq >>> 0, 4);
    Buffer.from(payload).copy(b, 8);
    return b;
  };
  reply(frame([0x90, 0x50, 0x02, 0xff]));     // COMPLETE (inquiry reply), power = 0x02 (on)
}

// VISCA over UDP
for (const view of VIEWS.filter((v) => v.transport === 'udp')) {
  const sock = dgram.createSocket('udp4');
  sock.on('message', (msg, rinfo) => {
    parseVisca(msg, view, (out) => sock.send(out, rinfo.port, rinfo.address));
  });
  sock.on('error', (e) => console.error(`cam${view.id + 1} udp error`, e.message));
  sock.bind(view.port, () => console.log(`cam${view.id + 1}: VISCA  UDP  :${view.port}`));
}

// VISCA over TCP (raw payloads, may arrive fragmented -- split on 0xFF terminator)
for (const view of VIEWS.filter((v) => v.transport === 'tcp')) {
  const srv = net.createServer((sock) => {
    let acc = Buffer.alloc(0);
    sock.on('data', (chunk) => {
      acc = Buffer.concat([acc, chunk]);
      let end;
      while ((end = acc.indexOf(0xff)) !== -1) {
        const frame = acc.subarray(0, end + 1);
        acc = acc.subarray(end + 1);
        parseVisca(frame, view, (out) => sock.write(out));
      }
    });
    sock.on('error', () => {});
  });
  srv.listen(view.port, () => console.log(`cam${view.id + 1}: VISCA  TCP  :${view.port}`));
}

// ---------------------------------------------------------------------------
// ONVIF (HTTP POST /onvif/PTZ with SOAP body)
// ---------------------------------------------------------------------------
function parseOnvif(body, view) {
  if (/<Stop\b/.test(body)) {
    logCmd(view.id, 'ONVIF stop');
    broadcast({ kind: 'stopall', cam: view.id });
    return;
  }
  let m;
  if ((m = body.match(/<GotoPreset[\s\S]*?<PresetToken>\s*(\d+)/))) {
    logCmd(view.id, `ONVIF goto preset ${m[1]}`);
    broadcast({ kind: 'preset', cam: view.id, action: 'goto', n: Number(m[1]) });
    return;
  }
  if ((m = body.match(/<SetPreset[\s\S]*?<PresetToken>\s*(\d+)/))) {
    logCmd(view.id, `ONVIF set preset ${m[1]}`);
    broadcast({ kind: 'preset', cam: view.id, action: 'set', n: Number(m[1]) });
    return;
  }
  if (/<ContinuousMove\b/.test(body)) {
    const pt = body.match(/<PanTilt\b[^>]*\bx="(-?[\d.]+)"[^>]*\by="(-?[\d.]+)"/);
    if (pt) {
      const pan = clamp1(Number(pt[1])), tilt = clamp1(Number(pt[2]));
      logCmd(view.id, `ONVIF move pan=${pan.toFixed(2)} tilt=${tilt.toFixed(2)}`);
      broadcast({ kind: 'pt', cam: view.id, pan, tilt });
      return;
    }
    const z = body.match(/<Zoom\b[^>]*\bx="(-?[\d.]+)"/);
    if (z) {
      const zoom = clamp1(Number(z[1]));
      logCmd(view.id, `ONVIF zoom ${zoom.toFixed(2)}`);
      broadcast({ kind: 'zoom', cam: view.id, zoom });
      return;
    }
  }
}
function clamp1(x) { return Math.max(-1, Math.min(1, x || 0)); }

const ONVIF_RESPONSE =
  '<?xml version="1.0" encoding="UTF-8"?>' +
  '<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"><s:Body/></s:Envelope>';

for (const view of VIEWS.filter((v) => v.transport === 'http')) {
  const srv = http.createServer((req, res) => {
    let body = '';
    req.on('data', (c) => (body += c));
    req.on('end', () => {
      parseOnvif(body, view);
      res.writeHead(200, { 'Content-Type': 'application/soap+xml; charset=utf-8' });
      res.end(ONVIF_RESPONSE);
    });
  });
  srv.listen(view.port, () => console.log(`cam${view.id + 1}: ONVIF  HTTP :${view.port}`));
}

// ---------------------------------------------------------------------------
// ATEM Mini switcher simulation (Blackmagic protocol, UDP 9910)
// ---------------------------------------------------------------------------
const F_ACKREQ = 0x01, F_HELLO = 0x02, F_RESEND = 0x04, F_REQNEXT = 0x08, F_ACK = 0x10;

const atem = {
  sock: dgram.createSocket('udp4'),
  client: null,        // { address, port }
  session: 0x0b06,
  localId: 0,
  program: 1,          // ATEM input source currently on Program
  preview: 2,          // ...and Preview
  pingTimer: null,
  unacked: new Map(),  // reliable packets awaiting ACK (id -> {body, tries})

  // Returns the 12-byte packet header. `lenField` is the total packet length
  // written into the header (body is concatenated separately by the caller).
  header(flags, lenField, ackedId = 0, localId = 0) {
    const b = Buffer.alloc(12);
    b[0] = (flags << 3) | ((lenField >> 8) & 0x07);
    b[1] = lenField & 0xff;
    b[2] = (this.session >> 8) & 0xff;
    b[3] = this.session & 0xff;
    b[4] = (ackedId >> 8) & 0xff;
    b[5] = ackedId & 0xff;
    b[10] = (localId >> 8) & 0xff;
    b[11] = localId & 0xff;
    return b;
  },

  send(buf) {
    if (this.client) this.sock.send(buf, this.client.port, this.client.address);
  },

  // Build one command segment: [len(2)][0,0][name(4)][data]
  cmd(name, data) {
    const seg = Buffer.alloc(8 + data.length);
    seg.writeUInt16BE(8 + data.length, 0);
    seg.write(name, 4, 4, 'latin1');
    data.copy(seg, 8);
    return seg;
  },

  inPr(source, longName, shortName) {
    const d = Buffer.alloc(30);
    d.writeUInt16BE(source, 0);
    Buffer.from(longName).copy(d, 2, 0, 20);
    Buffer.from(shortName).copy(d, 22, 0, 4);
    return this.cmd('InPr', d);
  },

  prgPrv(name, source) {
    const d = Buffer.alloc(4);
    d[0] = 0;                    // ME 0
    d.writeUInt16BE(source, 2);
    return this.cmd(name, d);
  },

  // Send a reliable (AckRequest) packet carrying command segments.
  // Send a reliable command packet and remember it until the client ACKs, so a
  // lost live update (e.g. a PGM/PVW change) gets retransmitted -- like a real
  // ATEM. (The init dump is handled separately via RequestNextAfter.)
  sendCommands(segments) {
    this.localId = (this.localId + 1) & 0xffff;
    const id = this.localId;
    const body = Buffer.concat(segments);
    this.sendReliable(id, body, false);
    this.unacked.set(id, { body, tries: 0 });
    return id;
  },

  sendReliable(id, body, resend) {
    const flags = F_ACKREQ | (resend ? F_RESEND : 0);
    this.send(Buffer.concat([this.header(flags, 12 + body.length, 0, id), body]));
  },

  sendPing() {
    this.localId = (this.localId + 1) & 0xffff;
    this.send(this.header(F_ACKREQ, 12, 0, this.localId));
  },

  handshake() {
    // Hello answer: 12-byte header + 8-byte payload; status byte (offset 12) = 0x02 (accepted)
    const hello = Buffer.concat([this.header(F_HELLO, 20, 0, 0), Buffer.alloc(8)]);
    hello[12] = 0x02;
    this.send(hello);
    // Initial state dump as packet id 1 (kept so we can resend it if the client
    // asks via RequestNextAfter), then a bare 12-byte packet id 2 (id>1) to
    // signal "init done".
    this.initStateBody = Buffer.concat([
      this.inPr(1, 'Camera 1', 'CAM1'),
      this.inPr(2, 'Camera 2', 'CAM2'),
      this.inPr(3, 'Camera 3', 'CAM3'),
      this.inPr(4, 'Camera 4', 'CAM4'),
      this.prgPrv('PrgI', this.program),
      this.prgPrv('PrvI', this.preview),
    ]);
    this.sendInitState();
    this.localId = 2;
    this.send(this.header(F_ACKREQ, 12, 0, 2));
    logCmd(null, `handshake done (session 0x${this.session.toString(16)}), PGM=${this.program} PVW=${this.preview}`);
  },

  // Send the stored init state dump as packet id 1 (literal id -- does not touch
  // the running localId counter, so it can be resent mid-stream).
  sendInitState() {
    const b = this.initStateBody;
    this.send(Buffer.concat([this.header(F_ACKREQ, 12 + b.length, 0, 1), b]));
  },

  setProgram(input) {
    this.program = input;
    const id = this.sendCommands([this.prgPrv('PrgI', input)]);
    logCmd(null, `PROGRAM -> input ${input} (atem pkt id ${id})`);
    broadcast({ kind: 'atem', program: this.program, preview: this.preview });
  },
  setPreview(input) {
    this.preview = input;
    const id = this.sendCommands([this.prgPrv('PrvI', input)]);
    logCmd(null, `PREVIEW -> input ${input} (atem pkt id ${id})`);
    broadcast({ kind: 'atem', program: this.program, preview: this.preview });
  },

  start() {
    // Reliable delivery: resend any packet the client hasn't ACKed (every 100ms,
    // marked as a retransmit), giving up after ~3s -- as a real ATEM does.
    setInterval(() => {
      for (const [id, e] of this.unacked) {
        if (e.tries >= 30) { this.unacked.delete(id); continue; }
        e.tries++;
        this.sendReliable(id, e.body, true);
      }
    }, 100);

    this.sock.on('message', (msg, rinfo) => {
      if (msg.length < 12) return;
      const flags = msg[0] >> 3;
      this.client = { address: rinfo.address, port: rinfo.port };
      if (flags & F_HELLO) {
        logCmd(null, `controller HELLO from ${rinfo.address}:${rinfo.port}`);
        this.unacked.clear();  // fresh session
        this.handshake();
        clearInterval(this.pingTimer);
        // Periodic keepalive, like a real ATEM (client drops the link after ~5s
        // of silence and reconnects).
        this.pingTimer = setInterval(() => this.sendPing(), 1000);
        return;
      }
      // Reliable delivery: the client ACKs what it received -> drop from the
      // retransmit queue. On RequestNextAfter it's telling us it missed an init
      // packet (bytes[6..7] = the id before the wanted one) -> resend it.
      const ackedId = (msg[4] << 8) | msg[5];
      if (flags & F_ACK) this.unacked.delete(ackedId);
      if (flags & F_REQNEXT) {
        const wantId = (((msg[6] << 8) | msg[7]) + 1) & 0xffff;
        if (wantId === 1 && this.initStateBody) this.sendInitState();
        else this.send(this.header(F_ACKREQ, 12, 0, wantId));
      }
    });
    this.sock.on('error', (e) => console.error('atem error', e.message));
    this.sock.bind(ATEM_PORT, () => console.log(`atem : Blackmagic UDP :${ATEM_PORT}`));
  },
};
atem.start();

// ---------------------------------------------------------------------------
// ONVIF WS-Discovery responder. Answers a multicast Probe with one ProbeMatch
// per ONVIF camera, each advertising its own port at this host's LAN IP -- so
// several cameras are discoverable on one IP.
// ---------------------------------------------------------------------------
function probeMatch(view, relatesTo) {
  const xaddr = `http://${LAN_IP}:${view.port}/onvif/device_service`;
  return Buffer.from(
    '<?xml version="1.0" encoding="UTF-8"?>' +
    '<e:Envelope xmlns:e="http://www.w3.org/2003/05/soap-envelope" ' +
    'xmlns:w="http://schemas.xmlsoap.org/ws/2004/08/addressing" ' +
    'xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery" ' +
    'xmlns:dn="http://www.onvif.org/ver10/network/wsdl">' +
    '<e:Header>' +
    `<w:MessageID>urn:uuid:sim-reply-cam${view.id + 1}-${Date.now()}</w:MessageID>` +
    `<w:RelatesTo>${relatesTo}</w:RelatesTo>` +
    '<w:To>http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous</w:To>' +
    '<w:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/ProbeMatches</w:Action>' +
    '</e:Header>' +
    '<e:Body><d:ProbeMatches><d:ProbeMatch>' +
    `<w:EndpointReference><w:Address>urn:uuid:sim-cam${view.id + 1}</w:Address></w:EndpointReference>` +
    '<d:Types>dn:NetworkVideoTransmitter</d:Types>' +
    `<d:Scopes>onvif://www.onvif.org/name/Camera${view.id + 1} onvif://www.onvif.org/hardware/SIM</d:Scopes>` +
    `<d:XAddrs>${xaddr}</d:XAddrs>` +
    '<d:MetadataVersion>1</d:MetadataVersion>' +
    '</d:ProbeMatch></d:ProbeMatches></e:Body></e:Envelope>'
  );
}

const wsd = dgram.createSocket({ type: 'udp4', reuseAddr: true });
wsd.on('message', (msg, rinfo) => {
  const s = msg.toString('utf8');
  if (!/Probe/.test(s) || /ProbeMatch/.test(s)) return; // it's a Probe, not our own reply
  const mid = s.match(/<[\w:]*MessageID>\s*([^<\s]+)/);
  const relatesTo = mid ? mid[1] : 'urn:uuid:unknown';
  const onvifViews = VIEWS.filter((v) => v.transport === 'http');
  for (const view of onvifViews) wsd.send(probeMatch(view, relatesTo), rinfo.port, rinfo.address);
  logCmd(null, `WS-Discovery probe from ${rinfo.address} -> announced ${onvifViews.map((v) => 'cam' + (v.id + 1)).join(', ')}`);
});
wsd.on('error', (e) => console.error('wsd error', e.message));
wsd.bind(WSD_PORT, () => {
  try { wsd.addMembership('239.255.255.250'); } catch (e) { console.error('wsd membership:', e.message); }
  console.log(`wsd  : ONVIF WS-Discovery UDP :${WSD_PORT}`);
});

// ---------------------------------------------------------------------------
console.log('\nLAN addresses for your ChurchCam config:');
for (const [name, addrs] of Object.entries(os.networkInterfaces())) {
  for (const a of addrs) if (a.family === 'IPv4' && !a.internal) console.log(`   ${name}: ${a.address}`);
}
console.log('');
