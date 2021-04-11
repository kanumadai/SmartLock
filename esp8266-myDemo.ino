#include <WiFiEsp.h>
#include <WiFiEspClient.h>
#include <WiFiEspServer.h>
#include "WiFiEspUdp.h"
#include <EEPROM.h>
#include "SoftwareSerial.h"
#define ledPin 13
#define ledWork 12
SoftwareSerial softserial(4, 5); // RX, TX
//--------------------------
//
char  deviceId[10]; 
//--------------------------
//ap mode ssid and pass        
char ssidAp[] = "Esp_Wifi";      
char passAp[] = "12345678"; 
WiFiEspServer serverAp(80);
RingBuffer buf(64);
//--------------------------
//--------------------------
// Initialize the Ethernet client object ,connect to the web server
WiFiEspClient severClient;
char* serverAddr="smartLockServer";
unsigned long lastConnectionTime = 0;         // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 10000L; // delay between updates, in milliseconds
//--------------------------
//link to the wifi,wifi's ssid and pass
char  ssid[32];            // your network SSID (name)
char  pass[32];                        // your network password
      
int status = WL_IDLE_STATUS;     // the Wifi radio's status
char cWifiInfo[64];
bool currentLineIsBlank = true;
bool bWifiDone=false;  
char* resp;
//--------------------------
// udp transmit test
WiFiEspUDP Udp;
unsigned int localPort = 8888;              // local port to listen on
char packetBuffer[5];  
//--------------------------
const   char*  respHttpHeadOk  ="HTTP/1.1 200 OK\r\nContent-type:text/html\r\n\r\n";
const   char*  respHttpHeadOk204 ="HTTP/1.1 204 OK\r\nContent-type:text/html\r\n\r\n";
const    char*  respHttpHeadError ="HTTP/1.1 400 OK\r\nContent-type:text/html\r\n\r\n";
const    char*  htmlHead = "<!DOCTYPE HTML>\r\n<html>\r\n<body>\r\n";
const    char*  htmlEnd = "<body>\r\n<html>\r\n";

const    char*  cSendHtmlForm ="<form><h4>SSID:<input type=\"text\" name=\"ssid\" value=\"\"/><br><h4>PASS:<input type=\"password\" name=\"pass\" value=\"\"/><br><input type=\"submit\" value=\"Submit\"/></form>";
const    char*  cSendHtmlOk ="<h4>Wifi set successful!</h4>\r\n";

//--------------------------
//cmd to server: login
const    char*  cCmdLogin = "GET /?eqid=00000011&reqtype=login HTTP/1.1";
//cmd to server: heartBeat
const    char*  cCmdHeartBeat = "GET /?eqid=00000011&reqtype=hartbeat HTTP/1.1";
//order from server:open
char *cOrderOpen = "open";
//order from server:close
char *cOrderClose = "close";
//--------------------------

void setup()
{
  // initialize serial for debugging
  Serial.begin(115200);
  // initialize serial for ESP module
  softserial.begin(9600);
  // initialize ESP module
  WiFi.init(&softserial);

  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    while (true); // don't continue
  }
  memset(ssid,'\0',32);
  memset(pass,'\0',32);
  //read saved WiFi info from flash
  // Serial.println();
  Serial.println(F("read saved WiFi info from the memery. "));
  readEeprom();
  // if ( status == WL_FAILURE) {
  //     Serial.println("Fialed to get WiFi info from the memery. ");
  // }
  while (true)
  {
    int iTry =1; 
    //Serial.println();
    Serial.println("ssid:");
    Serial.println(ssid);
    Serial.println("pass:");
    Serial.println(pass);  
    if(connectToWifi(iTry)){
      break;
    }      
    
    setWifiInfo();
    //save to eeprom
    updateEeprom(ssid,pass);
    WiFi.init(&softserial);   // initialize ESP module
  }
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("WiFi shield not present");
    while (true); // don't continue
  }

  // Udp.begin(localPort);

  //   // Serial.println("You're connected to the network");
  //   Serial.print("Beging UDP translation!, target port: ");
  //   Serial.println(localPort);
  status=0;
}

//status
//0,device must first login to the server ,status ->1
//1,waitting for the login answer. ok -> 2, fail ->0
//2,waitting for the command ,send heartbeat request.


void loop()
{
  //status 0, login to server
  if(status==0){
    //equitment is login into server
    sendHttpRequest(severClient,cCmdLogin);
    //status 1 ,wait for login answer
    status =1;
  }
  buf.init();  
  // if there's incoming data from the net connection send it out the serial port
  // this is for debugging purposes only
  while (severClient.available()) {
    char c = severClient.read();
    Serial.write(c);
    buf.push(c);

    if (buf.endsWith("\r\n\r\n") ) {
      if(status==4){
        sendHttpResponse(severClient,resp); 
        status=2;     
      }
      if(status==3){
        status=2;    
      }
      break;   
    }
    if(c=='\n'){
      char* cmd;
      int iRet;
      //status 2 ,wait for cmd        
      if(status ==2){        
        iRet = recvHttpRequest(cmd);
        //0,get a cmd; -1 ,no cmd
        if(iRet ==0){
          //status 4 ,got cmd,wait for get whole ack respose
          status=4;
          runCommd(cmd);
          resp="Ok";
        }
      }
      //status 1 ,wait for login answer
      if(status ==1){
        iRet = recvHttpResponse();
        //get resp,but not ok
        if(iRet ==0){
          status=0;
        }
        //get resp, ok
        if(iRet ==1){
          //status 3 ,login ok,wait for get whole ack respose
          status=3;
          //status led ,green ,always on
          digitalWrite(ledWork, HIGH) ;
        }
      }
    }
  }
  // if 2 seconds have passed since your last connection,
  // then connect again and send data
  if (millis() - lastConnectionTime > postingInterval) {
    if(status ==2)
      sendHttpRequest(severClient,cCmdHeartBeat);
  }
}

//get wifi info from eeprom
int readEeprom(){

  int addr=0;
  char value;
  while(true){
      value = EEPROM.read(addr);
    //  Serial.print('.');
      if(addr<32)
        ssid[addr]=value;
      if(addr>=32&&addr<64)
        pass[addr-32]=value;
      if(addr>=64)
        deviceId[addr-64]=value;
      if(addr==74){
        break;
      }
      addr++;
  }
  return WL_SUCCESS;
}

//update wifi info to eeprom,the max size of id or pa is 32 byte. 
int updateEeprom(char* id,char*pa){
  for(int n=0;n<32;n++){
      EEPROM.write(n, id[n]);
      delay(100);
  }    
    //  Serial.print('.');
  for(int n=32;n<64;n++){
      EEPROM.write(n, pa[n-32]);
      delay(100);
  }
     // Serial.print('.');
 // Serial.println();
  return WL_SUCCESS;
}

//print Wifi status
void printWifiStatus()
{
  // print the SSID of the network you're attached to
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  
  // print where to go in the browser
  // Serial.println();
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}

//html page
void sendHttpResponse(WiFiEspClient client,char* msg)
{
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line:
  client.print(respHttpHeadOk);
  client.print(htmlHead);
  client.print(msg);
  client.print(htmlEnd);  
  
  // The HTTP response ends with another blank line:
  client.println();
}

// this method makes a HTTP connection to the server
void sendHttpRequest(WiFiEspClient client,char* msg)
{    

  // close any connection before send a new request
  // this will free the socket on the WiFi shield
  client.stop();

  // if there's a successful connection
  if (client.connect(serverAddr, 80)) {
    Serial.println("Connecting...");
    
    // send the HTTP PUT request
    client.println(msg);
    client.println("Host: arduino.cc");
    client.println("Connection: close");
    client.println();

    // note the time that the connection was made
    lastConnectionTime = millis();
  }
  else {
    // if you couldn't make a connection
    Serial.println("Connection failed");
  }
}

//get data from net
int recvHttpRequest(char* cmd){
//"GET /?eqid=00000011&reqtype=hartbeat HTTP/1.1"
  int iRet=-1;
  buf.getStrN(cWifiInfo,0,64);
  if(startsWith(cWifiInfo,"GET /?")){ 
    char* token;
    token = strtok(cWifiInfo, "=");
    token = strtok(NULL, "&");
    //request is for this device
    if(deviceId==token){
      token = strtok(NULL, "=");
      token = strtok(NULL, " ");
      strcpy(cmd,token);
      iRet=0;
    }
  }  
  memset(cWifiInfo,"",strlen(cWifiInfo));
  return iRet;
}

int recvHttpResponse(){
  //"HTTP/1.1 200 OK";
  int iRet=-1;
  buf.getStrN(cWifiInfo,0,64);
  if(startsWith(cWifiInfo,"HTTP/1.1")){ 
    iRet=0;
    char* token;
    token = strtok(cWifiInfo, " ");
    token = strtok(NULL, " ");
    int iAnswer = atoi(token);
    //request is for this device
    if(iAnswer>=200 && iAnswer<400){
      iRet=1;
    }
  }  
  memset(cWifiInfo,"",strlen(cWifiInfo));
  return iRet;
}

void runCommd(char* cmd){
  if(cOrderOpen==cmd){
    digitalWrite(ledPin, HIGH) ;
  }
  if(cOrderClose==cmd){
    digitalWrite(ledPin, LOW) ;
  }

}



//connect to your wifi
bool connectToWifi(int iTryTimes){
    // attempt to connect to WiFi network
    bool bRet= false;
  while ( iTryTimes > 0) {
      Serial.print("Attempting to connect to WPA SSID: ");
      Serial.println(ssid);
      // Connect to WPA/WPA2 network
      status = WiFi.begin(ssid, pass);
      if ( status != WL_CONNECTED) {
        iTryTimes--;
      }
      else{
       // Serial.print("Connected to SSID: ");         
        //Serial.println(ssid);
        printWifiStatus();
 
        // if you get a connection, report back via serial:  
        bRet = true;
        break;
      }
  }
  return bRet;
}

//start ap,and to set wifi infomation
void setWifiInfo(){

  Serial.print("Fialed to connect to WiFi,Attempting to start AP ");
  Serial.println(ssidAp);
  // start access point
  status = WiFi.beginAP(ssidAp, 10, passAp, ENC_TYPE_WPA2_PSK);

  Serial.println("Access point started");
  printWifiStatus();
  
  // start the web server on port 80
  serverAp.begin();
  Serial.println("Server started: Please set the Wifi ssid and pass， Server addr and port .");

  int iRetry=0;

  while(!bWifiDone){
    // listen for incoming clients
    WiFiEspClient client = serverAp.available();
    if (client) {
      Serial.println("New client");
      // an http request ends with a blank line
      buf.init();  
      while (client.connected()) {
        if (client.available()) {
          char c = client.read();
          Serial.write(c);
          buf.push(c);  

          // you got two newline characters in a row
          // that's the end of the HTTP request, so send a response
          if (buf.endsWith("\r\n\r\n") && currentLineIsBlank) {
            Serial.print("Sending response:Wifi setting.Retry:");
            Serial.println(iRetry);
            sendHttpResponse(client,cSendHtmlForm);
            currentLineIsBlank=false;
            break;
          }
        //GET /?ssid=SPWH_H32_71D972&pass=h179q2fetbaedah HTTP/1.1
  
          if(c=='\n' && !bWifiDone){
            buf.getStrN(cWifiInfo,0,64);
            if(startsWith(cWifiInfo,"GET /?")){ 
              bWifiDone = true; 
              char* token;
              token = strtok(cWifiInfo, "=");
              token = strtok(NULL, "&");
              strcpy(ssid,token);
              token = strtok(NULL, "=");
              token = strtok(NULL, " ");
              strcpy(pass,token);
            } 
            else{
              memset(cWifiInfo,"",strlen(cWifiInfo));
            }         
          }
          if (buf.endsWith("\r\n\r\n") && currentLineIsBlank==false && bWifiDone) {
            // you're starting a new line
              sendHttpResponse(client,cSendHtmlOk);
            break;
          }
          if (buf.endsWith("\r\n\r\n") && currentLineIsBlank==false && !bWifiDone) {
            // you're starting a new line
            currentLineIsBlank=true;
            iRetry++;
            break;
          } 
        }
      }
      delay(10);

      // close the connection:
      client.stop();
      Serial.println("Client disconnected");
      if(bWifiDone){
        break;
      }
    }
  }
}

//chet string from the beginning.
bool startsWith(char* buff,const char* str)
{
	int findStrLen = strlen(str);

	// b is the start position into the ring buffer
	char* p1 = (char*)&str[0];
	char* p2 = p1 + findStrLen;

	for (char* p = p1; p < p2; p++)
	{
		if (*p != *buff)
			return false;
		buff++;
	}
	return true;
}
