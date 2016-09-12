#include <BasicTerm.h>
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <Thread.h>
#include <ThreadController.h>
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>
#include <MemoryFree.h>

boolean DEBUG = false;

#define ROWS 24
#define COLS 80

#define OBJECTS 2
#define RELAIS 8
const int BUFFERSIZE = (8 + 10 * OBJECTS) + (8 + 10 * RELAIS);
const unsigned char relaisPins[RELAIS] = {19, 18, 17, 16, 15, 5, 6, 7};
boolean relaisState[RELAIS] = {false, false, false, false, false, false, false, false};

#define MENUY 1
#define MENUX 59
#define MENUITEMS 3
const char* const menuItems[MENUITEMS] = {"Toggle debug", "Turn all ON", "Turn all OFF"};

ThreadController threadController = ThreadController();
Thread alarmsThread = Thread();
Thread ethernetThread = Thread();
Thread terminalThread = Thread();

static byte mac[6] = {0xDE, 0xAD, 0xC0, 0xDE, 0x00, 0x01};
const IPAddress ip(172, 16, 0, 2);
EthernetServer server(80);

BasicTerm terminal(&Serial);
uint32_t currentMillis;

unsigned char menuPosition = 0;


void loop()
{
  threadController.run();
}

void alarmsDelay()
{
  Alarm.delay(0);
}


void setPin(unsigned char _pin, boolean _state, boolean _redraw)
{
  relaisState[_pin] = _state;
  digitalWrite(relaisPins[_pin], _state ? LOW : HIGH);
  
  if (_redraw)
    drawStats();
}

void setPins(boolean _state)
{
  for (unsigned char _relais = 0; _relais < RELAIS; _relais++)
    setPin(_relais, _state, false);
  
  drawStats();
}

void togglePin(unsigned char _pin)
{
  setPin(_pin, !boolean(relaisState[_pin]), true);
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
  String _request;
  boolean _blank = true;
  
  while (_client.connected())
  {
    if (_client.available())
    {
      char _char = _client.read();

      _request = _request + _char;
          
      if (_char == '\n' && _blank)
        return (_request.substring(5, 12) != "favicon");
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

  StaticJsonBuffer<BUFFERSIZE> _jsonBuffer;
  
  JsonObject& _json = _jsonBuffer.createObject();
  JsonObject& _relaisData = _json.createNestedObject("relais");
  
  for (unsigned char _relais = 0; _relais < RELAIS; _relais++)
    _relaisData[String(_relais)] = relaisState[_relais];
  
  _json.printTo(_client);
}

void ethernetServer()
{
  EthernetClient _client = server.available();
  
  if (_client)
  {
    noInterrupts();
    
    if (readRequest(_client))
      writeResponse(_client);
      
    _client.stop();
    
    interrupts();
  }
}


void serialTerminal()
{
  uint32_t last = currentMillis;
  currentMillis  = millis();
  drawLatency(currentMillis - last);
  
  //uint16_t _key = terminal.get_key();


  handleKeys(terminal.get_key());

  if (DEBUG)
  {
    terminal.position(24, 0);
    terminal.set_attribute(BT_NORMAL);
    terminal.print(BUFFERSIZE);
  
    terminal.position(24, 4);
    terminal.print(freeMemory());
  }
}

void handleKeys(uint16_t _key)
{
  switch(_key)
  {
    case '\f':
      /* Ctrl-L: redraw screen */
      clearTerminal();
      break;
    
    case 13:
      /* RETURN: execute menu item */
      switch (menuPosition)
      {
        case 0:
          if (DEBUG)
          {
            terminal.set_color(BT_BLACK, BT_WHITE);
            terminal.position(24, 0);
            
            for (unsigned char _col = 1; _col < 10; _col++)
              terminal.print(F(" "));
          }
          
          DEBUG = !DEBUG;
          break;

        case 1:
          setPins(true);
          break;
          
        case 2:
          setPins(false);
          break;
      }
      break;

    case BT_KEY_UP:
      /* UP: menu up */
      if (menuPosition > 0)
        updateMenu(menuPosition - 1);
      break;

    case BT_KEY_DOWN:
      /* DOWN: menu down */
      if (menuPosition < MENUITEMS - 1)
        updateMenu(menuPosition + 1);
      break;
    
    default:
      if (_key >= 49 && _key <= 56)
        togglePin(_key - 49);
      break;
  }
}

void clearTerminal()
{
  terminal.cls();
  terminal.show_cursor(false);
  
  terminal.position(0, 0);
  terminal.set_attribute(BT_NORMAL);
  terminal.set_color(BT_BLACK, BT_WHITE);
  
  for (unsigned char _row = 0; _row < ROWS + 1; _row++)
  {
    terminal.position(_row, 0);
    
    for (unsigned char _col = 0; _col < COLS + 1; _col++)
      terminal.print(F(" "));
  }

  drawTerminal();
}

void drawTerminal()
{
  terminal.position(1, 2);
  terminal.set_attribute(BT_NORMAL);
  terminal.set_attribute(BT_BOLD);
  terminal.set_attribute(BT_UNDERLINE);
  terminal.set_color(BT_YELLOW, BT_BLACK);
  terminal.print(F("Jarvis Status Monitor"));
  
  terminal.set_attribute(BT_NORMAL);
  terminal.set_attribute(BT_BOLD);
  terminal.set_color(BT_WHITE, BT_BLACK);
  terminal.position(24, 67);
  terminal.print(F("Latency:"));

  terminal.position(7, 2);
  terminal.set_color(BT_CYAN, BT_BLACK);
  terminal.print(F("Relais:"));

  terminal.set_color(BT_WHITE, BT_BLACK);
  
  for (unsigned char _relais = 0; _relais < RELAIS; _relais++)
  {
    terminal.set_attribute(BT_BOLD);
    terminal.position(7 + _relais, 11);
    terminal.print(_relais + 1);
    
    terminal.set_attribute(BT_NORMAL);
    terminal.print(F(" = "));
  }
  
  drawStats();
  drawMenu();
  updateMenu(menuPosition);
  
  /*terminal.position(24, 16);

  for (unsigned char _color = 0; _color < 7; _color++)
  {
    terminal.set_attribute(BT_NORMAL);
    terminal.set_color(_color, BT_BLACK);
    terminal.print(_color);
    terminal.set_attribute(BT_BOLD);
    terminal.print(_color);
  }*/
}

void drawStats()
{
  terminal.set_attribute(BT_NORMAL);
  terminal.set_attribute(BT_BOLD);
  
  for (unsigned char _relais = 0; _relais < RELAIS; _relais++)
  {
    terminal.position(7 + _relais, 15);

    if (relaisState[_relais] == 0)
    {
      terminal.set_color(BT_RED, BT_BLACK);
      terminal.print(F("OFF"));
    }
    else if (relaisState[_relais] == 1)
    {
      terminal.set_color(BT_GREEN, BT_BLACK);
      terminal.print(F(" ON"));
    }
    else
    {
      terminal.set_attribute(BT_BLINK);
      terminal.set_color(BT_MAGENTA, BT_BLACK);
      terminal.print(F("ERR"));
    }
  }
}

void drawLatency(unsigned char _latency)
{
  if (_latency > 999)
    return;
    
  String _print = String(_latency) + " ms";
  unsigned char _printSize = _print.length();
  
  for (unsigned char _col = 0; _col < COLS - (74 + _printSize); _col++)
    _print = " " + _print;

  _printSize = _print.length();

  terminal.position(24, COLS - _printSize + 1);
  terminal.set_attribute(BT_NORMAL);
  terminal.set_attribute(BT_BOLD);
  
  if (_latency < 25)
    terminal.set_color(BT_GREEN, BT_BLACK);
  else if (_latency < 50)
    terminal.set_color(BT_YELLOW, BT_BLACK);
  else
    terminal.set_color(BT_RED, BT_BLACK);
  
  terminal.print(_print);
}

void drawMenu()
{
  terminal.position(MENUY, MENUX);

  terminal.set_attribute(BT_NORMAL);
  terminal.set_color(BT_BLACK, BT_WHITE);
  terminal.print(F("======[ "));

  terminal.set_attribute(BT_BOLD);
  terminal.set_color(BT_YELLOW, BT_WHITE);
  terminal.print(F("MENU"));

  terminal.set_attribute(BT_NORMAL);
  terminal.set_color(BT_BLACK, BT_WHITE);
  terminal.print(F(" ]======"));

  for (unsigned char _item = 0; _item < MENUITEMS + 2; _item++)
  {
    terminal.position(MENUY + 1 + _item, MENUX);
    terminal.print(F("|                  |"));
  }

  terminal.position(MENUY + 1 + MENUITEMS + 2, MENUX);
  terminal.print(F("===================="));

  terminal.set_color(BT_WHITE, BT_BLACK);

  for (unsigned char _item = 0; _item < MENUITEMS; _item++)
  {
    terminal.position(MENUY + 2 + _item, MENUX + 2);
    terminal.print(F("                "));

    terminal.position(MENUY + 2 + _item, MENUX + 2);
    terminal.print(menuItems[_item]);
  }
}

void updateMenu(unsigned char _position)
{
  terminal.set_attribute(BT_NORMAL);

  if (_position != menuPosition)
  {
    terminal.position(MENUY + 2 + menuPosition, MENUX + 2);
    terminal.print(menuItems[menuPosition]);
  }
  
  terminal.position(MENUY + 2 + _position, MENUX + 2);
  terminal.set_attribute(BT_BOLD);
  terminal.print(menuItems[_position]);
  
  menuPosition = _position;
}


void setupThread(ThreadController& _controller, Thread& _thread, unsigned char _interval, void (*_callback)(void))
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
  Serial.begin(115200);
  Ethernet.begin(mac, ip);
  server.begin();

  for (unsigned char _relais = 0; _relais < RELAIS; _relais++)
    pinMode(relaisPins[_relais], OUTPUT);
  
  setPins(false);
  
  terminal.init();

  clearTerminal();
  
  setupThread(threadController, alarmsThread, 1, alarmsDelay);
  setupThread(threadController, ethernetThread, 100, ethernetServer);
  setupThread(threadController, terminalThread, 1, serialTerminal);
  
  setupAlarms();

  currentMillis = now();
}
