#pragma once

#include <iostream>

#include "visa.h"
#include "visatype.h"
#include "casts.h"

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
