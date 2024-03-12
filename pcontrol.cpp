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

#include "curve_fit.hpp"
#include "gnuplot.h"

#include <gsl/gsl_randist.h>

#include "iniparser.hpp"

double gaussian(double x, double a, double b, double c)
{
    const double z = (x - b) / c;
    return a * std::exp(-0.5 * z * z);
}

double Landau(double x, double c, double mu, double sigma)
{
   if (sigma <= 0) return 0;
   double den = c * gsl_ran_landau_pdf( (x-mu)/sigma );
   return den;
}

struct histogr{
	std::vector<double> xvals;
	std::vector<int> yvals;
};

histogr FillHistogram(const std::vector<double>& data, double nbins){
	std::vector<double> xvals(nbins);
	std::vector<int> histo(nbins);
	double min_elem = *(std::min_element(data.begin(), data.end()));
	double max_elem = *(std::max_element(data.begin(), data.end()));
	double xmin = min_elem - 0.1 * min_elem;
	double xmax = max_elem + 0.1 * max_elem;
	double binwidth = (xmax - xmin) / nbins;
	for (size_t i = 0; i < nbins; i++) {
		xvals.at(i) = xmin + i * binwidth;
	};
	for (const double & item : data) {
		histo.at((size_t) ((item - xmin)/binwidth)) += 1;
	}
	histogr res {xvals, histo};
	return res;
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

	std::string xscale, yscale;

	INI::File fIniCfg;
	if (!fIniCfg.Load("../config.ini")) {
		std::cout << "Cannot open config file!\n";
	}

	bool bGetScreen = fIniCfg.GetSection("Main")->GetValue("bGetScreen").AsBool();
	bool bProcessData = fIniCfg.GetSection("Main")->GetValue("bProcessData").AsBool();
	bool bLeaveTerminal = fIniCfg.GetSection("Main")->GetValue("bLeaveTerminal").AsBool();
	std::string trigvalue = fIniCfg.GetSection("Main")->GetValue("fThreshold").AsString();
	nEvents = fIniCfg.GetSection("Main")->GetValue("nEvents").AsInt();

	std::string quevalue = fIniCfg.GetSection("Display")->GetValue("nChannel").AsString();
	xscale = fIniCfg.GetSection("Display")->GetValue("fHorizScale").AsString();
	yscale = fIniCfg.GetSection("Display")->GetValue("fVertScale").AsString();


	// Address of the oscilloscope, TCPIP or USB
	//std::string resourceString = "TCPIP0::192.168.0.200::inst0::INSTR";
	std::string resourceString = "USB0::0x0699::0x0527::C032360::INSTR";
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

	scpi = "horizontal:scale " + xscale;
	instrWrite(instr, scpi, retCount);
	scpi = "CH2:scale " + yscale;
	instrWrite(instr, scpi, retCount);

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
	scpi = "trigger:a:level:" + quevalue + " " + trigvalue;
	//instrWrite(instr, "trigger:a:level:ch2 0.5", retCount);
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

//	nEvents = 10;	//number of events to be processed, 10 if not specified
//	std::cout << "Enter the number of events to be registered:\n";
//	std::cin >> nEvents;

	std::vector<double> xv,yv, fv,av, iv;
	histogr fit_values, av_values, iv_values;

	//main data acquisition loop
	while (triggered < nEvents) {
		xv.clear();
		yv.clear();

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
							xv.push_back(xvalues[i-7]);
							yv.push_back(static_cast<double>((rdbuf[i] - yoff )*ymult + yzero));
					}
			}
			triggered++;	//increment number of registered events
			of.close();

			if (bProcessData) {
				auto fit_res = curve_fit(Landau, {0.4, -1e-9, 9e-8}, xv, yv);
				double integral = 0;
				for (auto& item : xv) {
					integral += Landau(item, fit_res[0], fit_res[1], fit_res[2]);
				}
				//std::cout << fit_res[1] << " " << fit_res[2] * 4 << "\n";
				fv.push_back(fit_res[2] * 4);
				av.push_back(fit_res[0]);
				iv.push_back(integral);
				//fit_values.yvals.push_back(fit_res[2] * 4);
			}
		}
		if ( triggered == 1 || triggered % 20 == 0 || triggered == nEvents) {	
			std::cout << "Processed " << triggered << "/" << nEvents << " events" << '\r';//control progress
		}
	}

		std::cout << '\n';
	if (bProcessData){
		fit_values = FillHistogram(fv, 500);
		av_values = FillHistogram(av,500);
		iv_values = FillHistogram(iv,500);
	}
	

	instrWrite(instr, "trigger:a:holdoff:by random", retCount);	//cancel delay between acquisitions
																//for fast acq screenshot
	if (bGetScreen) {
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
	}

	instrWrite(instr, "trigger:a:mode auto", retCount);			//turn back auto trigger mode
	instrWrite(instr, "trigger:a:level:ch2 0.5", retCount);

	viClose(instr);
	viClose(defaultRM);

	if (bProcessData) {
		std::ofstream out;
		out.open("data.txt", std::ios::trunc);
		for(size_t i = 0; i < fit_values.xvals.size(); i++)
		{
			out << std::scientific << std::setprecision(5) << fit_values.xvals[i] << "," << fit_values.yvals[i] << "\n";
		}
		out.close();
		out.open("amp.txt", std::ios::trunc);
		for(size_t i = 0; i < av_values.xvals.size(); i++)
		{
			out << std::scientific << std::setprecision(5) << av_values.xvals[i] << "," << av_values.yvals[i] << "\n";
		}
		out.close();
		out.open("intg.txt", std::ios::trunc);
		for(size_t i = 0; i < av_values.xvals.size(); i++)
		{
			out << std::scientific << std::setprecision(5) << iv_values.xvals[i] << "," << iv_values.yvals[i] << "\n";
		}
		out.close();

		GnuplotPipe gp;
		gp.sendLine("set datafile separator \",\"");
		//std::string gpquery = "plot \"./" + dirname + "/data_" + std::to_string(nEvents) 
		//	+ ".csv\" using 1:2 with lines, \"data.txt\" using 1:2 with lines";
		gp.sendLine("set term wxt 1 size 500, 400");
		std::string gpquery = "plot \"data.txt\" using 1:2 with lines";
		gp.sendLine(gpquery);
		gp.sendLine("set term wxt 2 size 500, 400");
		gpquery = "plot \"amp.txt\" using 1:2 with lines";
		gp.sendLine(gpquery);
		gp.sendLine("set term wxt 3 size 500, 400");
		gpquery = "plot \"intg.txt\" using 1:2 with lines";
		gp.sendLine(gpquery);
	}
	if (!bLeaveTerminal) {
		std::cout << "Done! Press 1 and hit ENTER to finish.\n";
		int a;
		std::cin >> a;
	}
	return 0;
}