//-------------------------------------------------------------------------------
//
//-------------------------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <cstdint>
#include <vector>
#include <fstream>
#include <iomanip>
#include <filesystem>

#include "visa.h"
#include "visatype.h"
#include "casts.h"
#include "vi_c2cpp.h"

int main() {

	ViStatus  status;
	ViSession defaultRM, instr;
	ViUInt32 retCount;
	ViChar buffer[80000];
	int recordLength, pt_off;
	double xinc, xzero, ymult, yzero, yoff;
	int triggered;
	size_t nEvents;

	// Address of the oscilloscope, TCPIP or USB
	std::string resourceString = "TCPIP0::192.168.0.200::inst0::INSTR";
	std::string scpi;

	// Initialize VISA session, if any errors, quit the program
	if (InitVisaSession(defaultRM) != 0) return 0;
	// Open instrument connection, if any errors, quit the program
	if (ConnectToInstrument(defaultRM, resourceString, VI_NULL, VI_NULL, instr, buffer) != 0) return 0;
	// Set timeout for queries/reading
	status = viSetAttribute(instr, VI_ATTR_TMO_VALUE, 10000);

	//Check if the connection to the instrument is established
	instrQuery(instr, "*idn?", retCount, buffer);
	std::cout << buffer << '\n';
	//set oscilloscope initial settings
	instrWrite(instr, "header 0", retCount);			//turn off headers for queries, so only arguments are returned
	instrWrite(instr, "data:source ch2", retCount);		//data from CH2 of osc
	instrWrite(instr, "data:enc sri", retCount);		//encoding of data SRIbinary (see specifications)
	instrWrite(instr, "data:width 1", retCount);		//data pieces are 1 byte wide

	instrWrite(instr, "data:start 1", retCount);		//starting data point
	instrWrite(instr, "data:stop 1e10", retCount);		//ending data point
	viSetAttribute(instr, VI_ATTR_TERMCHAR, '\r');		//termination char for output

	recordLength = atoi(instrQuery(instr, "WFMOutpre:NR_Pt?", retCount, buffer));	//number of points in waveform
	scpi = "data:stop " + std::to_string(recordLength);								//set proper ending data point
	instrWrite(instr, scpi, retCount);												//write it into osc

	//values necessary to reconstruct waveform: starting/ending/step of x,y
	xinc = std::atof(instrQuery(instr, "WFMOutpre:XINcr?", retCount, buffer));
	xzero = std::atof(instrQuery(instr, "WFMOutpre:XZERO?", retCount, buffer));
	pt_off = std::atof(instrQuery(instr, "WFMOutpre:PT_OFF?", retCount, buffer));
	
	ymult = std::atof(instrQuery(instr, "WFMOutpre:YMULT?", retCount, buffer));
	yzero = std::atof(instrQuery(instr, "WFMOutpre:YZERO?", retCount, buffer));
	yoff = std::atof(instrQuery(instr, "WFMOutpre:YOFF?", retCount, buffer));

	//set trigger source, level, edge
	instrWrite(instr, "trigger:a:edge:source ch2", retCount);
	instrWrite(instr, "trigger:a:level:ch2 0.04", retCount);
	instrWrite(instr, "trigger:a:edge:slope rise", retCount);

	triggered = 0;			//number of registered events
	std::string dump;
	ViInt8 rdbuf[recordLength + 8];
	
	//fill vector of time values, it will be used for each dataset
	std::vector<double> xvalues;
	double t0 = (-pt_off * xinc) + xzero;
	for (size_t i_t0 = 0; i_t0 < recordLength; i_t0++){
		xvalues.push_back(t0 + xinc * i_t0);
	}
	std::string filename;			//output file name

	std::string nSens;
	std::cout << "Enter the number of sensor or its string indentifier: \n";
	std::cin >> nSens;

	//create directory for output files
	std::string dirname = "sens " + nSens;
	std::filesystem::create_directory(dirname);
	std::filesystem::current_path("./" + dirname);

	nEvents = 10;	//number of events to be processed, 10 if not specified
	std::cout << "Enter the number of events to be registered:\n";
	std::cin >> nEvents;

	//main data acquisition loop
	while (triggered < nEvents) {
		instrQuery(instr, "trigger:state?", retCount, buffer);	//check trigger state
		dump.assign(buffer, retCount);
		//set trigger mode
		instrWrite(instr, "trigger:a:mode normal", retCount);
		instrWrite(instr, "trigger:a:holdoff:by time", retCount);	//set delay of 10 ms between events
		instrWrite(instr, "trigger:a:holdoff:time 0.01", retCount);	//to avoid recording same waveforms

		instrQuery(instr, "trigger:state?", retCount, buffer);	//on "trigger" state process waveform
		dump.assign(buffer, retCount);
		if (dump == "TRIGGER\n") {
			
			instrWrite(instr, "data:encdg ribinary", retCount);	//set ribinary data encoding
			instrWrite(instr, "curve?", retCount);
			//read from buffer data, number of data pieces are number of points (record length) +
			//8 bytes of ieee header
			viRead(instr, reinterpret_cast<ViUInt8*>(&rdbuf[0]), recordLength + 8, &retCount);
			instrWrite(instr, "*WAI", retCount);

			//create file with spectrum
			std::ofstream of;
			filename = "data_" + std::to_string(triggered + 1) + ".csv";
			of.open(filename, std::ofstream::out | std::ofstream::trunc);

			//check ieee format to skip first 8 bytes of data
			//ieee format: #<number of digits representing number of points><number of pts><data>
			//i.e.: #<5><62500><-27 -28 0 3 4 ...>
			//values in rdbuf are signed int8_t from -127 to 127
			
			size_t divider = 1; //write each divider-th count from waveform to reduce size of data file
			while (recordLength/divider > 20000) {
				divider++;
			}
			//skip first 8 bytes of data, write waveform in csv file
			for (size_t i = 7; i < (sizeof(rdbuf) - 1); i++) {
					if (i % divider == 0) {
						of << std::setprecision(5) << xvalues[i-7] << ","
							<< static_cast<double>((rdbuf[i] - yoff )*ymult + yzero) << '\n';
					}
			}

			triggered++;	//increment number of registered events
			of.close();
		}
		if ( triggered == 1 || triggered % 20 == 0 || triggered == nEvents) {	
			std::cout << "Processed " << triggered << "/" << nEvents << " events" << '\r';//control progress
		}
	}
	
	std::cout << '\n';
	instrWrite(instr, "trigger:a:holdoff:by random", retCount);	//cancel delay between acquisitions
																//for fast acq screenshot
	instrWrite(instr, "trigger:a:level:ch2 0.01", retCount);
	instrWrite(instr, "acquire:fastacq:state on", retCount);	//turn on fastacq mode
	instrWrite(instr, "pause 0.5", retCount);					//wait 0.5 s for screen image to establish
	
	instrWrite(instr, "save:image \"C:/st.png\"\n", retCount); //save image into osc memory
	instrWrite(instr, "*wai", retCount);						//wait for data to be written

	viSetAttribute(instr, VI_ATTR_TMO_VALUE, 25000);			//set larger timeout 25 s
	instrWrite(instr, "filesystem:readfile \"C:/st.png\"", retCount);	//read image in binary format from osc memory
	std::string fname = "!st.png";		//create file on local storage in which data is written
	std::ofstream of_pic;
	ViChar* picbuf[16000];				//buffer for picture file
	of_pic.open(fname, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);//open file in binary output mode
	viRead(instr, reinterpret_cast<unsigned char*>(&picbuf[0]), sizeof(picbuf),&retCount);//read data from osc buffer
	instrWrite(instr, "*wai", retCount);													//wait for data to be written in buffer

	of_pic.write(reinterpret_cast<char*> (picbuf), sizeof(picbuf));	//write data in output file
	of_pic.close();
	std::cout << "Screenshot captured successfully\n";

	instrWrite(instr, "acquire:fastacq:state off", retCount);	//turn off fast acquisition mode
	instrWrite(instr, "trigger:a:mode auto", retCount);			//turn back auto trigger mode

	std::filesystem::current_path("../");

	viClose(instr);
	viClose(defaultRM);
	std::cout << "Done! Press 1 and hit ENTER to finish.\n";
	int a;
	std::cin >> a;
	return 0;
}