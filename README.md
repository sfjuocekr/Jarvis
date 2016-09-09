# Jarvis
Home automation terminal server

# Usage
Read some code, modify some code and figure out what serial port to use!

Example:

stty -F /dev/ttyACM0 cs8 9600 ignbrk -brkint -imaxbel -opost -onlcr -isig -icanon -iexten -echo -echoe -echok -echoctl -echoke noflsh -ixon -crtscts
screen /dev/ttyACM0 9600

It "should" work with a ANSI/VT100 terminal, but you probably won't have a use for it anyway!
