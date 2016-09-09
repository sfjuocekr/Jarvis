#include <BasicTerm.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <Thread.h>
#include <ThreadController.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#define DEBUG true

const unsigned int relaisPins[8] = {19, 18, 17, 16, 15, 5, 6, 7};


ThreadController threadController = ThreadController();

Thread alarmsThread = Thread();
Thread ethernetThread = Thread();
Thread terminalThread = Thread();

byte mac[] = {0xDE, 0xAD, 0xC0, 0xDE, 0x00, 0x01};
const IPAddress ip(192, 168, 178, 2);
EthernetServer server(80);

DynamicJsonBuffer jsonBuffer;
JsonObject& json = jsonBuffer.createObject();
JsonArray& relais = json.createNestedArray("relais");

BasicTerm terminal(&Serial);


void loop()
{
  threadController.run();
}

void alarmsDelay()
{
  Alarm.delay(0);
}


void setPin(unsigned int _pin, boolean _state)
{
  JsonArray& _array = relais[_pin];
             _array[1] = int(_state);

  relais[_pin] = _array;
  
  digitalWrite(_array[0], _state ? LOW : HIGH);
}

void setPins(boolean _state)
{
  for (unsigned int _i = 0; _i < relais.size(); _i++)
    setPin(_i, _state);
}

void togglePin(unsigned int _pin)
{
  JsonArray& _array = relais[_pin];

  setPin(_pin, !boolean(_array[1]));
}


/*String addDigits(int _input)
{
  if(_input < 10)
    return strings[0] + String(_input);
  else
    return String(_input);
}*/


boolean readRequest(EthernetClient& _client)
{
  String _request = "";
  boolean _blank = true;
  
  while (_client.connected())
  {
    if (_client.available())
    {
      char _char = _client.read();

      _request = _request + String(_char);
          
      if (_char == '\n' && _blank)
        return (_request.substring(5, 12) != F("favicon"));
      else
        _blank = (_char == '\n') || !(_char != '\r');
    }
  }
  
  return false;
}

void writeResponse(EthernetClient& _client)
{
  _client.println(F("HTTP/1.1 200 OK"));
  _client.println(F("Content-Type: application/json"));
  _client.println(F("Connection: close"));
  _client.println();

  json.prettyPrintTo(_client);
}

void ethernetServer()
{
  noInterrupts();
  
  EthernetClient _client = server.available();
  
  if (_client)
  {
    if (readRequest(_client))
      writeResponse(_client);
      
    _client.stop();
  }
  
  interrupts();
}


uint32_t current;
uint32_t last;
uint32_t latency;

void serialTerminal()
{
  last = current;
  current  = millis();
  latency = current - last;
  
  uint16_t _key = terminal.get_key();

  drawStats();
  
  if (latency <= 1000)
  {
    terminal.position(79, 65);
    terminal.set_attribute(BT_NORMAL);
    terminal.print(F("Latency: "));
    
    terminal.set_attribute(BT_BOLD);
    terminal.print(latency);
    terminal.print(F(" ms"));
  }

  noInterrupts();
  
  switch(_key)
  {
    case ' ':
      /* Spacebar: all OFF */
      setPins(false);
      break;
      
    case '1':
      /* 1: toggle */
      togglePin(0);
      break;
      
    case '2':
      /* 2: toggle */
      togglePin(1);
      break;
      
    case '3':
      /* 3: toggle */
      togglePin(2);
      break;
      
    case '4':
      /* 4: toggle */
      togglePin(3);
      break;
      
    case '5':
      /* 5: toggle */
      togglePin(4);
      break;
      
    case '6':
      /* 6: toggle */
      togglePin(5);
      break;
      
    case '7':
      /* 7: toggle */
      togglePin(6);
      break;
      
    case '8':
      /* 8: toggle */
      togglePin(7);
      break;
      
    case '\f':
      /* Ctrl-L: redraw screen */
      clearTerminal();
      break;
  }
  
  interrupts();
}

void clearTerminal()
{
  terminal.cls();
  
  terminal.position(0, 0);
  terminal.set_attribute(BT_NORMAL);
  terminal.set_color(BT_BLACK, BT_WHITE);
  
  for (int _row = 0; _row < 25; _row++)
  {
    for (int _col = 0; _col < 80; _col++)
    {
      terminal.position(_row, _col);
      terminal.print(F(" "));
    }
  }

  drawTerminal();
}

void drawTerminal()
{
  terminal.position(1, 29);
  terminal.set_attribute(BT_NORMAL);
  terminal.set_attribute(BT_BOLD);
  terminal.set_color(BT_GREEN, BT_BLACK);
  terminal.print(F("Jarvis Status Monitor"));
  
  terminal.position(4, 2);
  terminal.set_attribute(BT_BOLD);
  terminal.set_color(BT_WHITE, BT_BLACK);
  terminal.print(F("JSON data: "));

  terminal.position(79, 65);
  terminal.set_attribute(BT_NORMAL);
  terminal.print(F("Latency: "));
  
  terminal.position(79, 77);
  terminal.set_attribute(BT_BOLD);
  terminal.print(F(" ms"));
}

void drawStats()
{
  terminal.position(4, 13);
  terminal.set_attribute(BT_NORMAL);
  json.printTo(terminal);
}

void setupThread(ThreadController& _controller, Thread& _thread, unsigned int _interval, void (*_callback)(void))
{
  _thread.enabled = true;
  _thread.setInterval(_interval);
  _thread.onRun(_callback);
  
  _controller.add(&_thread);  
}


void setupAlarms()
{
  //Alarm.timerRepeat(1, yolo1);
  //Alarm.timerRepeat(2, yolo2);
}


void setup()
{
  Serial.begin(9600);

  for (int _i = 0; _i < int(sizeof(relaisPins) / sizeof(unsigned int)); _i++)
  {
    pinMode(relaisPins[_i], OUTPUT);
    
    JsonArray& _array = jsonBuffer.createArray();
               _array.add(relaisPins[_i]);
               _array.add(0);
    
    relais.add(_array);
  }
  
  setPins(false);
  
  Ethernet.begin(mac, ip);
  server.begin();
  
  setupThread(threadController, alarmsThread, 1, alarmsDelay);
  setupThread(threadController, ethernetThread, 100, ethernetServer);
  setupThread(threadController, terminalThread, 1, serialTerminal);
  
  setupAlarms();
  
  terminal.init();
  terminal.cls();
  terminal.show_cursor(false);

  clearTerminal();
  
  current = now();
}
