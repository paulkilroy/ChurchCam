#include "globals.h"

// https://blog.devmobile.co.nz/2021/08/26/security-camera-onvif-discovery/

// https://www.onvif.org/onvif/ver20/ptz/wsdl/ptz.wsdl

// https://github.com/MikhaelMIEM/Visca-Onvif-converter-server

// From https://specs.xmlsoap.org/ws/2005/04/discovery/ws-discovery.pdf & http://www.onvif.org/wp-content/uploads/2016/12/ONVIF_WG-APG-Application_Programmers_Guide-1.pdf

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