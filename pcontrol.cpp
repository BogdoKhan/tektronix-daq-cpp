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

	instrQuery(instr, "WFMOutpre?", retCount, buffer);
	std::cout << buffer << '\n';


	recordLength = atoi(instrQuery(instr, "WFMOutpre:NR_Pt?", retCount, buffer));
	scpi = "DAT:STOP " + std::to_string(recordLength);
	instrWrite(instr, scpi, retCount);

	xinc = std::atof(instrQuery(instr, "WFMOutpre:XINcr?", retCount, buffer));
	xzero = std::atof(instrQuery(instr, "WFMOutpre:XZERO?", retCount, buffer));
	pt_off = std::atof(instrQuery(instr, "WFMOutpre:PT_OFF?", retCount, buffer));
	
	ymult = std::atof(instrQuery(instr, "WFMOutpre:YMULT?", retCount, buffer));
	yzero = std::atof(instrQuery(instr, "WFMOutpre:YZERO?", retCount, buffer));
	yoff = std::atof(instrQuery(instr, "WFMOutpre:YOFF?", retCount, buffer));

	std::cout << ymult << " " << yzero << " " << yoff << '\n';

	instrWrite(instr, "trigger:a:edge:source ch2", retCount);
	instrWrite(instr, "trigger:a:level:ch2 0.05", retCount);
	instrWrite(instr, "trigger:a:edge:slope rise", retCount);

	triggered = 0;
	std::string dump;
	ViInt8 rdbuf[26000];
	std::vector<double> r1;
	while (triggered < 1) {
		instrQuery(instr, "trigger:state?", retCount, buffer);
		dump.assign(buffer, retCount);
		//std::cout << dump << " " << retCount << '\n';

		instrWrite(instr, "trigger:a:mode normal", retCount);
		instrWrite(instr, "trigger:a:holdoff:by time", retCount);
		instrWrite(instr, "trigger:a:holdoff:time 0.05", retCount);

		instrQuery(instr, "trigger:state?", retCount, buffer);
		dump.assign(buffer, retCount);
		if (dump == "TRIGGER\n") {
			
			instrWrite(instr, "data:encdg ribinary", retCount);
			//instrQuery(instr, "curve?", retCount, rdbuf);
			instrWrite(instr, "curve?", retCount);
			viRead(instr, reinterpret_cast<ViUInt8*>(&rdbuf[0]), 25000, &retCount);
			//std::cout << retCount << '\n';
			instrWrite(instr, "*WAI", retCount);
			//fprintf(stderr, "scanf: %f\n", buffer);
			//std::cout << static_cast<std::uint8_t>(rdbuf) << '\n';
			//std::stringstream ss;
			//dump.assign(rdbuf, 25008);
			//std::string dd (rdbuf);

			std::ofstream of;
			of.open("test.dat", std::ofstream::out | std::ofstream::trunc);
			for (size_t i = 0; i < sizeof(rdbuf); i++) {
					r1.push_back((rdbuf[i] - yoff)*ymult - yzero);
			}
			for (auto i : r1) {

				of << i << "\n" ;
			}
			std::cout << '\n';

			triggered++;
			of.close();
		}


	}


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