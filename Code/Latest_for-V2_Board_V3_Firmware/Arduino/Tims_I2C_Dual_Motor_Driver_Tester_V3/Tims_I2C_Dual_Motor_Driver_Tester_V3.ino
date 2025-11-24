
/*
	A small program to send commands via Serial to Tim's I2C Dual Motor Driver V3.

	By Tim Jackson.1960
		More info on Instructables. https://www.instructables.com/member/Palingenesis/instructables/


	ESP8266 (2M SPIFFS)
		Variables and constants in RAM (global, static), used 29504 / 80192 bytes (36%)
			SEGMENT  BYTES    DESCRIPTION
			DATA     1516     initialized variables
			RODATA   1364     constants
			BSS      26624    zeroed variables
		Instruction RAM (IRAM_ATTR, ICACHE_RAM_ATTR), used 62223 / 65536 bytes (94%)
			SEGMENT  BYTES    DESCRIPTION
			ICACHE   32768    reserved space for flash instruction cache
			IRAM     29455    code in IRAM
		Code in flash (default, ICACHE_FLASH_ATTR), used 243028 / 1048576 bytes (23%)
			SEGMENT  BYTES    DESCRIPTION
			IROM     243028   code in flash

	Arduino NANO ATMega328p
		Sketch uses 8628 bytes (28%) of program storage space. Maximum is 30720 bytes.
		Global variables use 968 bytes (47%) of dynamic memory, leaving 1080 bytes for local variables. Maximum is 2048 bytes.


	Wire request buffer of the Motor Driver was originaly set/configured for 32 bit values but was reduced to 24+ bit numbers.
	The original code is commented out should I change microcontroller with more memory.
	I say 24+ bit number becouse I have used a seperate Byte for negative flags.
	This gives a number range of: -16,777,215 to 16,777,215.
	To make it more universal, all values are Ticks of the Motors Quadratic Encoder.

	Wire_Request converts as follows:

	Flags	(2 bytes Used to hold 16 single bit values like negative switches and flags)

		Slave_Buffer_Tx[0] and Slave_Buffer_Tx[1]
		Hold Flags

		Flags_1 true if:
			LSB		1	=	0x01 	Is_400K (default = 100K)
					2	=	0x02	Has_Station_A
					4	=	0x04	Has_Station_B
					8	=	0x08
					16	=	0x10
					32	=	0x20
					64	=	0x40
			MSB		128	=	0x80

		Flags_2 true if:
			LSB		1	=	0x01
					2	=	0x02
					4	=	0x04
					8	=	0x08
					16	=	0x10
					32	=	0x20
					64	=	0x40
			MSB		128	=	0x80

		Current positions (signed 32 bit, 4 bytes)
		Slave_Buffer_Tx[2] = CurPosition_A
		Slave_Buffer_Tx[6] = CurPosition_B

		I2C Address (unsigned 8 bit, 1 bytes)
		Slave_Buffer_Tx[10] = Requested_I2C_Address

		Stations (signed 16 bit, 2 bytes)
		Slave_Buffer_Tx[11] = Station_F_A
		Slave_Buffer_Tx[13] = Station_F_B
		Slave_Buffer_Tx[15] = Station_R_A
		Slave_Buffer_Tx[17] = Station_R_B

		Motor Loads (unsigned 16 bit. 2 bytes)
		Slave_Buffer_Tx[19] = _Val_A
		Slave_Buffer_Tx[21] = _Val_B


	The buffer holds 32 bytes, the rest are not used.
		Currently hold data version an maker.

Below is just for me to use as I use Visual Studio 2022 Comunity to edit code.
#include <Tims_Arduino_NANO.h>
*/

/*	Debug	*/
//#define DEBUG
#define SERIAL_PRINT

/*	Use ADC to read Motor Loads	*/
//#define USE_ADC

/*	Arduino IDE	*/
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/*	LCD	*/
LiquidCrystal_I2C lcd(0x27, 16, 2);

//#define TARGET_I2C_ADDRESS	0x30    /*  0x30=48 0x60=96 0x32=50Target Address.	*/
int Target_I2C_Address = 0x30;

#define BUFF_64				64      /*  What is the longest message Arduino can store?	*/
#define BUFF_16				16
#define BUFF_24				24
#define BUFF_32				32

#define SDA_PIN 4
#define SCL_PIN 5
const int16_t I2C_MASTER = 0x42;
const int16_t I2C_SLAVE = 0x08;

/*	SERIAL MESSAGES	*/
char Buffer_TX[BUFF_64];		/* message				*/
uint8_t Buffer_RX[BUFF_32];		/* message				*/
unsigned int no_data = 0;
unsigned int sofar;				/* size of Buffer_TX	*/
bool isComment = false;
bool procsessingString = false;
short cmd = -1;
bool slaveProcesing = false;
bool ProcsessingCommand = false;

/*	Debug	*/
long TimeNow = millis();
long Period = 1000;
long TimeOut = TimeNow + Period;

void setup() {
	/*
	  Start serial.
	*/
	Serial.begin(115200);
	/*
	  Start Wire.
		  Used only as Master atm. (address optional for master)
		  Wire.begin(SDA Pin, SCL Pin, Master Address);
	*/
	Wire.begin();
	Wire.setClock(400000);
	Wire.onRequest(Wire_Request);

	/*	LCD	*/
	lcd.init();
	lcd.backlight();
	lcd.print("   Tim's Dual   ");
	lcd.setCursor(0, 1);//bottom line
	lcd.print("  Motor Driver  ");



	/*	Debug	*/
	pinMode(LED_BUILTIN, OUTPUT);

}

void loop() {
	/*
		Tick - Tock
	*/
	TimeNow = millis();
	/*
		Get next command when finish current command.
	*/
	if (!procsessingString) { ReadSerial(); }

	if (TimeNow > TimeOut) {
		TimeOut = TimeNow + Period;
		digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
	}

}
/*
	Read Data in Serial Buffer if available.
*/
void ReadSerial() {
	if (Serial.available() > 0) {
		char c = Serial.read();
		no_data = 0;

		/*	Handle comments (G-code style)	*/
		if (c == '(' || c == ';') {
			isComment = true;
		}
		if (!isComment) {
			Buffer_TX[sofar++] = c;
		}
		if (c == ')') {
			isComment = false;
		}

		/*	Check for CRLF sequence	*/
		if (c == '\r') {
			delay(10);	/*	Short delay to allow LF to arrive	*/
			if (Serial.peek() == '\n') {
				Buffer_TX[sofar++] = Serial.read();	/*	consume LF	*/
				if (Serial.peek() == '\n'){
					Serial.read();	/*	consume extra LF	*/
				}

				/*	End of command detected	*/

				procsessingString = true;
				processCommand();
				init_process_string();
			}
		}
	}
}
/*
	Set ready for next command.
*/
void init_process_string() {

	memset(Buffer_TX, 0, BUFF_64);	/*	Clear transmit buffer	*/

	/*	Reset state variables	*/
	sofar = 0;
	procsessingString = false;
	isComment = false;
	cmd = -1;

	Serial.println("ok");	/*	Acknowledge ready for next command	*/

	/*	No flush or delay needed	*/

}
/*
	Change command numbers to a long.
*/
long Parse_Number(char code, long val) {
	/*
		start at the beginning of Buffer_TX.
	*/
	char* ptr = Buffer_TX;

	/*
	  Go char to char through string.
	*/
	while ((long)ptr > 1 && (*ptr) && (long)ptr < (long)Buffer_TX + sofar) {
		/*
			if you find code as you go through string.
		*/
		if (*ptr == code) {
			/*
				convert the digits that follow into a long and return it.
			*/
			return atol(ptr + 1);
		}
		/*
			take a step from here to the next char after the next space.
		*/
		ptr = strchr(ptr, ' ') + 1;
	}
	/*
		If the end is reached and nothing found, return val.
	*/
	return val;
}
/*
	Process Commands.
*/
void processCommand() {
	int32_t val32 = 0;
	int16_t val16 = 0;

	/*	Get I2C Address if given (default 0x30 or last sent)	*/
	Target_I2C_Address = Parse_Number('D', Target_I2C_Address);

	char* ptr = Buffer_TX;

	if (*ptr == 'X') {

#ifdef SERIAL_PRINT
		Serial.print("Buffer_TX ");
		Serial.print(Buffer_TX);
		Serial.print("   (");
		Serial.print(sofar);
		Serial.println(")");
		delay(50);
#endif  /*	SERIAL_PRINT	*/

		/*
			GET
				X					= gets all status info from the Motor Driver.
				X<number of bytes>	= gets number of bytes specified from the Motor Driver.
		
				Best values are:
					8	bytes		= Current Position A, Current Position B.
					9	bytes		= As above + I2C Address + I2C Speed.
					17	bytes		= As above + Station A and B info.
		
		*/
		int _numberOfBytes = Parse_Number('X', -1);
		if (_numberOfBytes == 0) _numberOfBytes = BUFF_32;

		/*	Clear RX buffer	*/
		for (size_t i = 0; i < BUFF_32; i++) Buffer_RX[i] = 0;

		Wire.requestFrom(Target_I2C_Address, _numberOfBytes);
		delayMicroseconds(10);

		for (size_t i = 0; i < _numberOfBytes && Wire.available(); i++) {
			Buffer_RX[i] = Wire.read();
		}

		/*	I2C Address check	*/
		if (Buffer_RX[10] == 0) {+
			Serial.println();
			Serial.print("No Address ");
			Serial.print(Target_I2C_Address, DEC);
			Serial.print(" (0x");
			Serial.print(Target_I2C_Address, HEX);
			Serial.println(") found.");
		}
		else {

			/*	Current Position A (int32)	*/
			val32 = (int32_t)Buffer_RX[2] |
				((int32_t)Buffer_RX[3] << 8) |
				((int32_t)Buffer_RX[4] << 16) |
				((int32_t)Buffer_RX[5] << 24);
			Serial.print("Current Position A = ");
			Serial.println(val32);

			/*	Current Position B (int32)	*/
			val32 = (int32_t)Buffer_RX[6] |
				((int32_t)Buffer_RX[7] << 8) |
				((int32_t)Buffer_RX[8] << 16) |
				((int32_t)Buffer_RX[9] << 24);
			Serial.print("Current Position B = ");
			Serial.println(val32);

			Serial.println();	/*	Add a Space	*/

			/*	I2C Address	*/
			Serial.print("I2C Address = ");
			Serial.print(Buffer_RX[10], DEC);
			Serial.print(" (0x");
			Serial.print(Buffer_RX[10], HEX);
			Serial.println(")");

			/*	I2C Speed	*/
			Serial.print("I2C Speed = ");
			Serial.println((Buffer_RX[0] & 0x01) ? "400000" : "100000");

			Serial.println();	/*	Add a Space	*/

			/*	Station A ON/OFF	*/
			Serial.print("Station A = ");
			Serial.println((Buffer_RX[0] & 0x02) ? "ON" : "OFF");

			/*	Station Forward A (int8)	*/
			val16 = (int8_t)Buffer_RX[11];
			Serial.print("Station A Forward = ");
			Serial.println(val16);

			/*	Station Reverse A (int8)	*/
			val16 = (int8_t)Buffer_RX[12];
			Serial.print("Station A Reverse = ");
			Serial.println(val16);

			/*	Acceleration A Rate (unsigned 16 bit)	*/
			val16 = (Buffer_RX[24] << 8) | Buffer_RX[23];
			Serial.print("Acceleration A = ");
			Serial.println(val16);

			/*	Encoder Slot Count (unsigned 16 bit)	*/
			val16 = (Buffer_RX[28] << 8) | Buffer_RX[27];
			Serial.print("Encoder A Slot Count = ");
			Serial.println(val16);

			Serial.println();	/*	Add a Space	*/

			/*	Station B ON/OFF	*/
			Serial.print("Station B = ");
			Serial.println((Buffer_RX[0] & 0x04) ? "ON" : "OFF");

			/*	Station Forward B (int8)	*/
			val16 = (int8_t)Buffer_RX[13];
			Serial.print("Station B Forward = ");
			Serial.println(val16);

			/*	Station Reverse B (int8)	*/
			val16 = (int8_t)Buffer_RX[14];
			Serial.print("Station B Reverse = ");
			Serial.println(val16);

			/*	Acceleration B Rate (unsigned 16 bit)	*/
			val16 = (Buffer_RX[26] << 8) | Buffer_RX[25];
			Serial.print("Acceleration B = ");
			Serial.println(val16);

			/*	Encoder B Slot Count (unsigned 16 bit)	*/
			val16 = (Buffer_RX[30] << 8) | Buffer_RX[29];
			Serial.print("Encoder Slot Count B = ");
			Serial.println(val16);

			Serial.println();	/*	Add a Space	*/

#ifdef USE_ADC
			/*	Motor Load A (uint16)	*/
			val16 = (Buffer_RX[20] << 8) | Buffer_RX[19];
			Serial.print("Motor Load A = ");
			Serial.println(val16);

			/*	Motor Load B (uint16)	*/
			val16 = (Buffer_RX[22] << 8) | Buffer_RX[21];
			Serial.print("Motor Load B = ");
			Serial.println(val16);

			Serial.println();	/*	Add a Space	*/
#endif

			Serial.println("Tim");
		}
	}
	else if (*ptr == 'P') {
		/*	Process command (future expansion)	*/
	}
	else {
		/*	SEND	*/
		ProcsessingCommand = true;
		SendBufferOnI2C(Target_I2C_Address);
		delay(4);
		ProcsessingCommand = false;
	}
}
/*
	Send Recived Commands on to a Device on the I2C bus.
*/
void SendBufferOnI2C(int I2C_address) {

	Wire.beginTransmission(I2C_address);	//	Get device at I2C_address attention
	Wire.write('#');						/*	Send garbage for the first byte of data that will be ignored.	*/
	Wire.write(Buffer_TX, sofar);			//	Send Buffer_TX
	Wire.endTransmission();					//	Stop transmitting

#ifdef SERIAL_PRINT
	Serial.print("Buffer_TX ");
	Serial.print(Buffer_TX);
	Serial.print("   (");
	Serial.print(sofar);
	Serial.println(")");
	delay(50);
#endif  /*	SERIAL_PRINT	*/

}
/*
	I2C Request Data
*/
void Wire_Request() {

}
