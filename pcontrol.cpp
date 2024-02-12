//-------------------------------------------------------------------------------
// Name:  Save Hardcopy with C++ using VISA for 2/3/4/5/6 Series Scopes
// Purpose : This example demonstrates how to save a hard copy screen image from
//   the scope to the PC.
//
// Created:  10/4/2023
//
// Compatible Instruments : 2/3/4/5/6 Series Riddick Based Oscilloscopes
//
// Compatible Interfaces : USB, Ethernet, GPIB
//
// Tektronix provides the following example "AS IS" with no support or warranty.
//
//-------------------------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <cstdint>
#include <vector>
#include <fstream>
#include <iomanip>
#include <filesystem>

#include "include/progressbar.hpp"
#include "include/visa.h"
#include "include/visatype.h"
#include "include/casts.h"

int InitVisaSession(ViSession& defaultRM) {
	ViStatus status;
	status = viOpenDefaultRM(&defaultRM);
	if (status < VI_SUCCESS) {
		std::cout << "Failed to initialize\nPress 0 and hit ENTER to quit\n";
		std::cin >> status;
		return -1;
	}
	else {
		std::cout << "Resource manager initialized successfuly\n";
	}
	return 0;
}

int ConnectToInstrument(
	ViSession& defaultRM, 
	const std::string& resourceString, 
	const ViUInt32& access_mode,
	const ViUInt32& timeout,
	ViSession& instr, 
	ViChar* buffer){

	ViStatus status;
	status = viOpen(defaultRM, resourceString.c_str(), VI_NULL, VI_NULL, &instr);
	if (status < VI_SUCCESS) {
		std::cout << "Error connecting to instrument\nPress 0 and hit ENTER to quit\n";
		std::cin >> status;
		viStatusDesc(defaultRM, status, buffer);
		std::cout << buffer << '\n';
		return 1;
	}
	else {
		std::cout << "Instrument initialized successfuly\n";
	}
	return 0;
}

int instrWrite(const ViSession& instr, std::string& scpi, ViUInt32& retCount) {
	ViStatus status;
	status = viWrite(instr,  str_to_uch(scpi), scpi.size(), &retCount);
	if (status < VI_SUCCESS) {
		printf("Error writing to instrument\n");
		return 2;
	}
	else return 0;
}

void instrWrite(const ViSession& instr, const char* command, ViUInt32& retCount) {
	std::string scpi (command);
	instrWrite(instr, scpi, retCount);
}

ViChar* instrRead(const ViSession& instr, ViChar* buffer, ViUInt32& retCount) {
	ViStatus status;
	ViUInt32 sbuf = 1024*1024;

	status = viRead(instr, reinterpret_cast<unsigned char*>(buffer), sbuf, &retCount);
	if (status < VI_SUCCESS) {
		printf("Error reading from instrument\n");
		return buffer;
	}
	else return buffer;
}

ViChar* instrQuery(const ViSession& instr, std::string& scpi, ViUInt32& retCount, ViChar* buffer) {
	ViStatus status;
	instrWrite(instr, scpi, retCount);
	instrRead(instr, buffer, retCount);
	return buffer;
}

ViChar* instrQuery(const ViSession& instr, const char* command, ViUInt32& retCount, ViChar* buffer) {
	std::string scpi (command);
	return instrQuery(instr, scpi, retCount, buffer);
}


int main() {

	ViStatus  status;
	ViSession defaultRM, instr;
	ViUInt32 retCount;
	ViChar buffer[80000];
	int recordLength, pt_off;
	double xinc, xzero, ymult, yzero, yoff;
	int triggered;
	size_t nEvents;

	// Modify the following line to configure this script for your instrument
	std::string resourceString = "TCPIP0::192.168.0.200::inst0::INSTR";
	std::string scpi;

	// Where to save (PC side)
	std::string target_path = "C:\\tempx\\";
	std::string target_name = "scope_screenshot.png";
	std::string full_path;

	// Initialize VISA session, if any errors, quit the program
	if (InitVisaSession(defaultRM) != 0) return 0;
	// Open instrument connection, if any errors, quit the program
	if (ConnectToInstrument(defaultRM, resourceString, VI_NULL, VI_NULL, instr, buffer) != 0) return 0;
	// Set timeout
	status = viSetAttribute(instr, VI_ATTR_TMO_VALUE, 10000);

	//Check if the connection to the instrument is established
	instrQuery(instr, "*idn?", retCount, buffer);
	std::cout << buffer << '\n';

	instrWrite(instr, "header 0", retCount);
	instrWrite(instr, "data:source ch2", retCount);
	instrWrite(instr, "data:enc sri", retCount);
	instrWrite(instr, "data:width 1", retCount);

	instrWrite(instr, "data:start 1", retCount);
	instrWrite(instr, "data:stop 1e10", retCount);
	viSetAttribute(instr, VI_ATTR_TERMCHAR, '\r');

	//instrQuery(instr, "WFMOutpre?", retCount, buffer);
	//std::cout << buffer << '\n';


	recordLength = atoi(instrQuery(instr, "WFMOutpre:NR_Pt?", retCount, buffer));
	scpi = "DAT:STOP " + std::to_string(recordLength);
	instrWrite(instr, scpi, retCount);

	xinc = std::atof(instrQuery(instr, "WFMOutpre:XINcr?", retCount, buffer));
	xzero = std::atof(instrQuery(instr, "WFMOutpre:XZERO?", retCount, buffer));
	pt_off = std::atof(instrQuery(instr, "WFMOutpre:PT_OFF?", retCount, buffer));
	
	ymult = std::atof(instrQuery(instr, "WFMOutpre:YMULT?", retCount, buffer));
	yzero = std::atof(instrQuery(instr, "WFMOutpre:YZERO?", retCount, buffer));
	yoff = std::atof(instrQuery(instr, "WFMOutpre:YOFF?", retCount, buffer));


	instrWrite(instr, "trigger:a:edge:source ch2", retCount);
	instrWrite(instr, "trigger:a:level:ch2 0.04", retCount);
	instrWrite(instr, "trigger:a:edge:slope rise", retCount);

	triggered = 0;			//number of registered events
	std::string dump;
	ViInt8 rdbuf[recordLength + 8];
	
	//fill vector of time values
	std::vector<double> xvalues;
	double t0 = (-pt_off * xinc) + xzero;
	for (size_t i_t0 = 0; i_t0 < recordLength; i_t0++){
		xvalues.push_back(t0 + xinc * i_t0);
	}
	std::string filename;


	std::string nSens;
	std::cout << "Enter the number of sensor or its string indentifier: \n";
	std::cin >> nSens;

	std::string dirname = "sens " + nSens;
	std::filesystem::create_directory(dirname);
	 std::cout << "Current path is " << std::filesystem::current_path() << '\n';
	std::filesystem::current_path("./" + dirname);
	 std::cout << "Current path is " << std::filesystem::current_path() << '\n';

	nEvents = 10;
	std::cout << "Enter the number of events to be registered:\n";
	std::cin >> nEvents;
	//progressbar prog_bar(nEvents);

	while (triggered < nEvents) {
		instrQuery(instr, "trigger:state?", retCount, buffer);
		dump.assign(buffer, retCount);
		//std::cout << dump << " " << retCount << '\n';

		instrWrite(instr, "trigger:a:mode normal", retCount);
		instrWrite(instr, "trigger:a:holdoff:by time", retCount);
		instrWrite(instr, "trigger:a:holdoff:time 0.01", retCount);

		instrQuery(instr, "trigger:state?", retCount, buffer);
		dump.assign(buffer, retCount);
		if (dump == "TRIGGER\n") {
			
			instrWrite(instr, "data:encdg ribinary", retCount);
			//instrQuery(instr, "curve?", retCount, rdbuf);
			instrWrite(instr, "curve?", retCount);
			//read from buffer data, number of data pieces are number of points (record length) +
			//8 bytes of ieee header
			viRead(instr, reinterpret_cast<ViUInt8*>(&rdbuf[0]), recordLength + 8, &retCount);
			instrWrite(instr, "*WAI", retCount);

			//create file with spectrum
			std::ofstream of;
			filename = "data_" + std::to_string(triggered + 1) + ".csv";
			of.open(filename, std::ofstream::out | std::ofstream::trunc);

			//skip first 8 bytes of data
			//ieee format: #<number of digits representing number of points><number of pts><data>
			//i.e.: #<5><62500><-27 -28 0 3 4 ...>
			//values in rdbuf are signed int8_t from -127 to 127
			//std::cout << "x_size " << xvalues.size() << " y size " << (sizeof(rdbuf) - 8) << '\n';
			
			size_t divider = 1; //write each divider-th count from waveform to reduce size of data
			while (recordLength/divider > 20000) {
				divider++;
			}

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
			std::cout << "Processed " << triggered << "/" << nEvents << " events" << '\r';
		}
	}
	std::cout << '\n';
	instrWrite(instr, "trigger:a:mode auto", retCount);
	instrWrite(instr, "trigger:a:holdoff:by random", retCount);

/*	instrWrite(instr, "trigger:a:level:ch2 0.01", retCount);
	instrWrite(instr, "acquire:fastacq:state on", retCount);
	instrWrite(instr, "pause 0.5", retCount);
	
	instrWrite(instr, "save:image \"C:/st.png\"", retCount);
	instrWrite(instr, "*wai", retCount);

	viSetAttribute(instr, VI_ATTR_TMO_VALUE, 25000);
	instrWrite(instr, "filesyste::readfile \"C:/st.png\"", retCount);
	std::string fname = "!st.png";
	std::ofstream of_pic;
	ViUInt16 picbuf[160000];
	of_pic.open(fname, std::ofstream::out | std::ofstream::trunc);
	viRead(instr, reinterpret_cast<ViPBuf>(&picbuf[0]), sizeof(picbuf),&retCount);
		instrWrite(instr, "*wai", retCount);

	for (size_t i = 0; i < sizeof(picbuf); i++) {
		of_pic << std::hex << picbuf[i];
	}
	of_pic.close();

	instrWrite(instr, "acquire:fastacq:state off", retCount);*/

	std::filesystem::current_path("../");


/*
	// Configure scope for screenshot, see programmers manual for scope-specific syntax
	// Setting where to save screenshot on scope
	scpi = "SAVE:IMAGE \"C:\\screenshots\\tek.png\"\n";
	const unsigned char* scpi_buf = reinterpret_cast<const unsigned char*>(scpi.c_str());
	status = viWrite(instr, scpi_buf, (ViUInt32)(scpi.length()), &retCount);
	if (status < VI_SUCCESS) {
		printf("Error writing to instrument\n");
		viStatusDesc(defaultRM, status, buffer);
		printf("%s\n", buffer);
						std::cin >> aa;
		return 0;
	}

	// Transfer screenshot to PC
	const unsigned char* vi_buf = reinterpret_cast<const unsigned char*>("FILESystem:READFile \"C:\\screenshots\\tek.png\"\n");
	status = viWrite(instr, vi_buf, 45, &retCount);
	if (status < VI_SUCCESS) {
		printf("Error writing to instrument\n");
		viStatusDesc(defaultRM, status, buffer);
		printf("%s\n", buffer);
		return 0;
	}
	status = viRead(instr, 
	reinterpret_cast<unsigned char*>(buffer), 
	sizeof(buffer), &retCount);
	if (status < VI_SUCCESS) {
		printf("Error reading from instrument\n");
		viStatusDesc(defaultRM, status, buffer);
		printf("%s\n", buffer);
		return 0;
	}

	snprintf(const_cast<char*>(full_path.c_str()),
	 sizeof(full_path), "%s%s", target_path, target_name);
	FILE* file = fopen(full_path.c_str(), "wb");
	if (file == NULL) {
		printf("Failed to open file for writing.\n");
		return -1;
	}

	size_t bytesWritten = fwrite(buffer, 1, retCount, file);

	// Check if the write operation was successful
	if (bytesWritten != atoi(buffer)) {
		perror("Error writing to the file");
		fclose(file);
		return 1; // Exit with an error code
	}

	fclose(file);*/

	viClose(instr);
	viClose(defaultRM);
	int a;
	std::cin >> a;
	return 0;
}