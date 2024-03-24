#include "globals.h"
#include <string.h>
#include <time.h>

// https://blog.devmobile.co.nz/2021/08/26/security-camera-onvif-discovery/

// https://www.onvif.org/onvif/ver20/ptz/wsdl/ptz.wsdl

// https://github.com/MikhaelMIEM/Visca-Onvif-converter-server

// From https://specs.xmlsoap.org/ws/2005/04/discovery/ws-discovery.pdf & http://www.onvif.org/wp-content/uploads/2016/12/ONVIF_WG-APG-Application_Programmers_Guide-1.pdf
/*
char WSDiscoveryProbeMessages[] =
"<?xml version = \"1.0\" encoding=\"UTF-8\"?>" 
"<e:Envelope xmlns:e=\"http://www.w3.org/2003/05/soap-envelope\" " 
        "xmlns:w=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" " 
        "xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" " 
        "xmlns:dn=\"http://www.onvif.org/ver10/network/wsdl\"> " 
    "<e:Header>"
        "<w:MessageID>"
            "dae35c8e-97df-49a5-9fc3-d0bcad1db922"
        "</w:MessageID>" 
        "<w:To e:mustUnderstand=\"true\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</w:To> " 
        "<w:Action mustUnderstand=\"true\">http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</w:Action> " 
    "</e:Header> " 
    "<e:Body> " 
        "<d:Probe> " 
            "<d:Types>dn:NetworkVideoTransmitter</d:Types>" 
        "</d:Probe> " 
    "</e:Body> " 
"</e:Envelope>";
*/

const char stopFormat[] = 
"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">\r\n"
"<s:Header>\r\n"
"<Security s:mustUnderstand=\"1\" xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">\r\n"
"<UsernameToken>\r\n"
"<Username>admin</Username>\r\n"
"<Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">Lv8CmS+6HqXZ/vGm8R7k1j2tKM4=</Password>\r\n"//TODO: make this with sha and base64
"<Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">f2+oEzPKt7++/EQZBog05OoqI9Q=</Nonce>\r\n"//TODO: find out what this is
"<Created xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">2024-02-21T22:06:08.000Z</Created>\r\n"//TODO: correctly generate the date
"</UsernameToken>\r\n"
"</Security>\r\n"
"</s:Header>\r\n"
"<s:Body>\r\n"
"<Stop xmlns=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
"<Zoom>%s</Zoom>\r\n"
"<ProfileToken>Profile_1</ProfileToken>\r\n"//TODO: read the profile token and plug it in here
"<PanTilt>%s</PanTilt>\r\n"
"</Stop>\r\n"
"</s:Body>\r\n"
"</s:Envelope>\r\n";

const char zoomFormat[] = 
"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">\r\n"
"<s:Header>\r\n"
"<Security s:mustUnderstand=\"1\" xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">\r\n"
"<UsernameToken>\r\n"
"<Username>admin</Username>\r\n"
"<Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">Lv8CmS+6HqXZ/vGm8R7k1j2tKM4=</Password>\r\n"//TODO: make this with sha and base64
"<Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">f2+oEzPKt7++/EQZBog05OoqI9Q=</Nonce>\r\n"//TODO: find out what this is
"<Created xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">2024-02-21T22:06:08.000Z</Created>\r\n"//TODO: correctly generate the date
"</UsernameToken>\r\n"
"</Security>\r\n"
"</s:Header>\r\n"
"<s:Body>\r\n"
"<ContinuousMove xmlns=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
"<ProfileToken>Profile_1</ProfileToken>\r\n"
"<Velocity>\r\n"
"<Zoom x=\"%1.2f\"xmlns=\"http://www.onvif.org/ver10/schema\">\r\n"
"</Zoom>\r\n"
"</Velocity>\r\n"
"</ContinuousMove>\r\n"
"</s:Body>\r\n"
"</s:Envelope>\r\n";

const char velFormat[] = 
"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">\r\n"
"<s:Header>\r\n"
"<Security s:mustUnderstand=\"1\" xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">\r\n"
"<UsernameToken>\r\n"
"<Username>admin</Username>\r\n"
"<Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">Lv8CmS+6HqXZ/vGm8R7k1j2tKM4=</Password>\r\n"//TODO: make this with sha and base64
"<Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">f2+oEzPKt7++/EQZBog05OoqI9Q=</Nonce>\r\n"//TODO: find out what this is
"<Created xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">2024-02-21T22:06:08.000Z</Created>\r\n"//TODO: correctly generate the date
"</UsernameToken>\r\n"
"</Security>\r\n"
"</s:Header>\r\n"
"<s:Body>\r\n"
"<ContinuousMove xmlns=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
"<ProfileToken>Profile_1</ProfileToken>\r\n"
"<Velocity>\r\n"
"<PanTilt xmlns=\"http://www.onvif.org/ver10/schema\" x=\"%1.3f\" y=\"%1.3f\"/>\r\n"
"</Velocity>\r\n"
"</ContinuousMove>\r\n"
"</s:Body>\r\n"
"</s:Envelope>\r\n";

const char setPresetFormat[] = 
"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">\r\n"
"<s:Header>\r\n"
"<Security s:mustUnderstand=\"1\" xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">\r\n"
"<UsernameToken>\r\n"
"<Username>admin</Username>\r\n"
"<Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">Lv8CmS+6HqXZ/vGm8R7k1j2tKM4=</Password>\r\n"//TODO: make this with sha and base64
"<Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">f2+oEzPKt7++/EQZBog05OoqI9Q=</Nonce>\r\n"//TODO: find out what this is
"<Created xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">2024-02-21T22:06:08.000Z</Created>\r\n"//TODO: correctly generate the date
"</UsernameToken>\r\n"
"</Security>\r\n"
"</s:Header>\r\n"
"<s:Body>\r\n"
"<SetPreset xmlns=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
"<ProfileToken>Profile_1</ProfileToken>\r\n"
"<PresetToken>%d</PresetToken>\r\n"
"</SetPreset>\r\n"
"</s:Body>\r\n"
"</s:Envelope>\r\n";

const char goToPresetFormat[] = 
"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">\r\n"
"<s:Header>\r\n"
"<Security s:mustUnderstand=\"1\" xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">\r\n"
"<UsernameToken>\r\n"
"<Username>admin</Username>\r\n"
"<Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">Lv8CmS+6HqXZ/vGm8R7k1j2tKM4=</Password>\r\n"//TODO: make this with sha and base64
"<Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">f2+oEzPKt7++/EQZBog05OoqI9Q=</Nonce>\r\n"//TODO: find out what this is
"<Created xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">2024-02-21T22:06:08.000Z</Created>\r\n"//TODO: correctly generate the date
"</UsernameToken>\r\n"
"</Security>\r\n"
"</s:Header>\r\n"
"<s:Body>\r\n"
"<GotoPreset xmlns=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
"<ProfileToken>Profile_1</ProfileToken>\r\n"
"<PresetToken>%d</PresetToken>\r\n"
"</GotoPreset>\r\n"
"</s:Body>\r\n"
"</s:Envelope>\r\n";

const char getPresetsFormat[] = 
"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">\r\n"
"<s:Header>\r\n"
"<Security s:mustUnderstand=\"1\" xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">\r\n"
"<UsernameToken>\r\n"
"<Username>admin</Username>\r\n"
"<Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">QkM4OtrhqKSTjc/StAi/sM6xafY=</Password>\r\n"//TODO: make this with sha and base64
"<Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">61YGabGnG1m4ie1RJotpl7wHmSQ=</Nonce>\r\n"//TODO: find out what this is
"<Created xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">2024-02-06T03:46:53.000Z</Created>\r\n"//TODO: correctly generate the date
"</UsernameToken>\r\n"
"</Security>\r\n"
"</s:Header>\r\n"
"<s:Body>\r\n"
"<GetPresets xmlns=\"http://www.onvif.org/ver20/ptz/wsdl\">\r\n"
"<ProfileToken>Profile_1</ProfileToken>\r\n"
"</GetPresets>\r\n"
"</s:Body>\r\n"
"</s:Envelope>\r\n";

const char httpFormat[] =
"POST /onvif/PTZ HTTP/1.1\r\n"
"Host: %d.%d.%d.%d\r\n"//Replace IP
"Accept: */*\r\n"
"User-Agent: curl/8.1.2\r\n"
"Connection: Keep-Alive\r\n"
"Content-Type: text/xml; charset=utf-8\r\n"
"Content-Length: %d\r\n"
"\r\n"
"%s";

char soapBuf[1000];
char messageBuf[1500];

int prevPan = 0, prevTilt = 0, prevZoom = 0;

/*
Velocity [PTZSpeed]
A Velocity vector specifying the velocity of pan, tilt and zoom.
PanTilt - optional; [Vector2D]
Pan and tilt speed. The x component corresponds to pan and the y component to tilt. If omitted in a request, the current (if any) PanTilt movement should not be affected.
Zoom - optional; [Vector1D]
A zoom speed. If omitted in a request, the current (if any) Zoom movement should not be affected.

<ns0:ContinuousMove xmlns:ns0="http://www.onvif.org/ver20/ptz/wsdl">
    <ns0:ProfileToken>MediaProfile00000</ns0:ProfileToken>
    <ns0:Velocity>
        <ns1:PanTilt xmlns:ns1="http://www.onvif.org/ver10/schema" x="0.041666666666666664" y="0"/>
        <ns2:Zoom xmlns:ns2="http://www.onvif.org/ver10/schema" x="0"/>
    </ns0:Velocity>
</ns0:ContinuousMove>
*/

void onvif_setup() {
    // cycle through each camera 
    // see if it is using onvif
    // if first octet is not 0
    // get ptz serivce name from device service
    // get profile token name
    // set presets 1 and 2
}

void onvif_send( int cameraNumber ) {
    //byte messageBuf[2048];
    if (NETWORK_SUCCESS != connect(cameraNumber)) {
        //VISCA_ERROR("E97");
        logi("Onvif could not connect");
    }

    if (NETWORK_SUCCESS != send(cameraNumber, (byte*)messageBuf, strlen(messageBuf))) {
        //VISCA_ERROR("E96");
        logi("Onvif could not send");
    }
    //logi("Message sent: %s", messageBuf);

    if (NETWORK_SUCCESS != recieve(cameraNumber, (byte*)messageBuf)) {
        logi("Unable to get response");
    } else {
        //logi("Message Recieved: %s", messageBuf);
    }

    closeConnection(cameraNumber);
}

void initialize() {
    Onvif_SetPreset(1);
    Onvif_SetPreset(2);
}

void Onvif_GoToPreset( int presetNumber ) {
    int cameraNumber = getActiveCamera();
    sprintf(soapBuf, goToPresetFormat, presetNumber);
    sprintf(messageBuf, httpFormat, settings.cameraIP[cameraNumber][0], settings.cameraIP[cameraNumber][1], settings.cameraIP[cameraNumber][2], settings.cameraIP[cameraNumber][3], strlen(soapBuf), soapBuf);
    onvif_send( cameraNumber );
}

void Onvif_GetPresets( int cameraNumber ) {
    sprintf(messageBuf, httpFormat, settings.cameraIP[cameraNumber][0], settings.cameraIP[cameraNumber][1], settings.cameraIP[cameraNumber][2], settings.cameraIP[cameraNumber][3], strlen(getPresetsFormat), getPresetsFormat);
    onvif_send( cameraNumber );
}

void Onvif_SetPreset( int presetNumber ) {
    int cameraNumber = getActiveCamera();
    sprintf(soapBuf, setPresetFormat, presetNumber);
    sprintf(messageBuf, httpFormat, settings.cameraIP[cameraNumber][0], settings.cameraIP[cameraNumber][1], settings.cameraIP[cameraNumber][2], settings.cameraIP[cameraNumber][3], strlen(soapBuf),soapBuf);
    onvif_send( cameraNumber );
}

void Onvif_ZoomDrive(int zoomSpeed) {
    
}

void Onvif_PanTiltDrive(int panSpeed, int tiltSpeed) {
    int cam = getActiveCamera();
    sprintf(soapBuf, velFormat, panSpeed, tiltSpeed);
    sprintf(messageBuf, httpFormat, settings.cameraIP[cam][0], settings.cameraIP[cam][1], settings.cameraIP[cam][2], settings.cameraIP[cam][3], strlen(soapBuf), soapBuf);
    logi("Sending pan tilt message x:%d y:%d", panSpeed, tiltSpeed);
    onvif_send(cam);
}

void Onvif_Stop(bool stopPanTilt, bool stopZoom) {
    int cam = getActiveCamera();
    sprintf(soapBuf, stopFormat, "true", "false");
    sprintf(messageBuf, httpFormat, settings.cameraIP[cam][0], settings.cameraIP[cam][1], settings.cameraIP[cam][2], settings.cameraIP[cam][3], strlen(soapBuf), soapBuf);
    logi("Stopping");
    onvif_send(cam);
}

void Onvif_PtzDrive( int panSpeed, int tiltSpeed, int zoomSpeed ) {
    //logi("curr p: %d, prev p:%d, curr t: %d, prev t: %d, curr z: %d, prev z: %d", panSpeed, prevPan, tiltSpeed, prevTilt, zoomSpeed, prevZoom);
    if ((panSpeed == prevPan) && (tiltSpeed == prevTilt) && (zoomSpeed == prevZoom)) { 
        //Serial.println("Not sending any message");
        return;
    }

    int cam = getActiveCamera();

    //char camIP[] = "192.168.4.200";
    //logi("%s","Creating buffers");
    //Values must be between -1 and 1

    if ((zoomSpeed == 0) && (prevZoom != 0)) {
        sprintf(soapBuf, stopFormat, "true", "false");
        sprintf(messageBuf, httpFormat, settings.cameraIP[cam][0], settings.cameraIP[cam][1], settings.cameraIP[cam][2], settings.cameraIP[cam][3], strlen(soapBuf), soapBuf);
        logi("Stopping zoom");
        onvif_send(cam);
    } else {
        sprintf(soapBuf, zoomFormat, zoomSpeed/7.0);
        sprintf(messageBuf, httpFormat, settings.cameraIP[cam][0], settings.cameraIP[cam][1], settings.cameraIP[cam][2], settings.cameraIP[cam][3], strlen(soapBuf), soapBuf);
        logi("Sending zoom message z:%d", zoomSpeed);
        onvif_send(cam);
    }
    //logi("cp:%d pp:%d ct:%d pt:%d", panSpeed, prevPan, tiltSpeed, prevTilt);
    if (((panSpeed == 0) && (tiltSpeed == 0)) && ((prevPan != 0) || (prevTilt != 0))) { 
        //Serial.println("Not sending any message");
        sprintf(messageBuf, httpFormat, settings.cameraIP[cam][0], settings.cameraIP[cam][1], settings.cameraIP[cam][2], settings.cameraIP[cam][3], strlen(stopFormat), stopFormat);
        logi("Stopping pan tilt");
        onvif_send(cam);
    } else {
        sprintf(soapBuf, velFormat, panSpeed/15.0, tiltSpeed/15.0);
        //Set previous values to check next time
    //logi("%s %d", velBuf, strlen(velBuf));
        sprintf(messageBuf, httpFormat, settings.cameraIP[cam][0], settings.cameraIP[cam][1], settings.cameraIP[cam][2], settings.cameraIP[cam][3], strlen(soapBuf), soapBuf);
    //logi("%s %d", messageBuf, strlen(messageBuf));
        logi("Sending pan tilt message x:%d y:%d", panSpeed, tiltSpeed);
        onvif_send(cam);
    }

    prevPan = panSpeed;
    prevTilt = tiltSpeed;
    prevZoom = zoomSpeed; 
    ///sprintf(headerBuf, headerFormat, 666);
    //printf("%s", headerBuf);  
    /*   
        char buffer[1024] = { 0 };
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        //return -1;
    }
 
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
 
    // Convert IPv4 and IPv6 addresses from text to binary
    // form
    if (inet_pton(AF_INET, "192.168.4.200", &serv_addr.sin_addr)
        <= 0) {
        printf(
            "\nInvalid address/ Address not supported \n");
        //return 0;
    }
 
    if ((status
         = connect(client_fd, (struct sockaddr*)&serv_addr,
                   sizeof(serv_addr)))
        < 0) {
        printf("\nConnection Failed \n");
        //return 0;
    }
    send(client_fd, messageBuf, strlen(messageBuf), 0);
    printf("move message sent\n");
    valread = read(client_fd, buffer,
                   1024 - 1); // subtract 1 for the null
                              // terminator at the end
    printf("%s\n", buffer);
    // closing the connected socket
    close(client_fd);
    */
}

/* GotoPreset??
[GotoPreset]
ProfileToken [ReferenceToken]
A reference to the MediaProfile where the operation should take place.
PresetToken [ReferenceToken]
A requested preset token.
Speed - optional; [PTZSpeed]
A requested speed.The speed parameter can only be specified when Speed Spaces are available for the PTZ Node.
PanTilt - optional; [Vector2D]
Pan and tilt speed. The x component corresponds to pan and the y component to tilt. If omitted in a request, the current (if any) PanTilt movement should not be affected.
Zoom - optional; [Vector1D]
A zoom speed. If omitted in a request, the current (if any) Zoom movement should not be affected.
*/