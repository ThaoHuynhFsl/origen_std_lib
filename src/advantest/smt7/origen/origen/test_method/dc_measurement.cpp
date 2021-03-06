#include "dc_measurement.hpp"

using namespace std;

namespace Origen {
namespace TestMethod {

// Defaults
DCMeasurement::DCMeasurement() {
	applyShutdown(1);
	checkShutdown(1);
	measure("VOLT");
	settlingTime(0);
	forceValue(0);
	iRange(0);
	processResults(1);
	badc(0);
	port("");
	clamp(0, 5); // clamp for TP360 release checker only support voltage measurement
}

DCMeasurement::~DCMeasurement() { }

DCMeasurement & DCMeasurement::applyShutdown(int v) { _applyShutdown = v; return *this; }
DCMeasurement & DCMeasurement::shutdownPattern(string v) { _shutdownPattern = v; return *this; }
DCMeasurement & DCMeasurement::checkShutdown(int v) { _checkShutdown = v; return *this; }
DCMeasurement & DCMeasurement::measure(string v) { _measure = v; return *this; }
DCMeasurement & DCMeasurement::settlingTime(double v) { _settlingTime = v; return *this; }
DCMeasurement & DCMeasurement::pin(string v) { _pin = v; return *this; }
DCMeasurement & DCMeasurement::port(string v) { _port = v; return *this; }
DCMeasurement & DCMeasurement::forceValue(double v) { _forceValue = v; return *this; }
DCMeasurement & DCMeasurement::iRange(double v) { _iRange = v; return *this; }
DCMeasurement & DCMeasurement::processResults(int v) { _processResults = v; return *this; }
DCMeasurement & DCMeasurement::badc(int v) { _badc = v; return *this; }
DCMeasurement & DCMeasurement::clamp(double v, double y) { _clampLo = v, _clampHi = y; return *this; }

// All test methods must implement this function
DCMeasurement & DCMeasurement::getThis() { return *this; }

void DCMeasurement::_setup() {
	pin(extractPinsFromGroup(_pin));
	results.resize(numberOfPhysicalSites + 1);
	funcResultsPre.resize(numberOfPhysicalSites + 1);
	funcResultsPost.resize(numberOfPhysicalSites + 1);
	label = Primary.getLabel();
}

void DCMeasurement::_execute() {

	int site;

	ON_FIRST_INVOCATION_BEGIN();

	if (_applyShutdown) {
		if (_shutdownPattern.empty()) {
			ostringstream pat;
			pat << label << "_part1";
			shutdownPattern(pat.str());
		}
	}

	if (!_iRange && _measure == "CURR") {
		double dHigh(0), dLow(0);
		TM::COMPARE cHigh, cLow;
		testLimits().TEST_API_LIMIT.get(cLow, dLow, cHigh, dHigh);

		if (cLow == TM::NA) {
			dLow = 0;
		}
		if (cHigh == TM::NA) {
			dHigh = 0;
		}

		if (dLow == 0 && dHigh == 0) {
			cout << "ERROR: If your current measurement does not have a limit, you must supply the current range" << endl;
			ERROR_EXIT(TM::ABORT_FLOW);
		}
		if (abs(dLow) > abs(dHigh)) {
			_iRange = abs(dLow);
		} else {
			_iRange = abs(dHigh);
		}
	}

	RDI_BEGIN(TA::SINGLE);

	if (_port.empty()) {
		rdi.func(suiteName + "f1").label(label).execute();
	} else {
		rdi.port(_port).func(suiteName + "f1").burst(label).execute();
	}

	callHoldState();

	if(_measure == "VOLT") {

		double dHigh(0), dLow(0);
		TM::COMPARE cHigh, cLow;
		testLimits().TEST_API_LIMIT.get(cLow, dLow, cHigh, dHigh);

		if (cLow == TM::NA) {
			dLow = 0;
		}
		if (cHigh == TM::NA) {
			dHigh = 0;
		}

		if (_badc) {
			if (_port.empty()) {
				rdi.dc(suiteName)
			    				   .pin(_pin, TA::BADC)
			    				   .measWait(_settlingTime)
			    				   .vMeas()
			    				   .execute();
			} else {
				rdi.port(_port).dc(suiteName)
			    				   .pin(_pin, TA::BADC)
			    				   .measWait(_settlingTime)
			    				   .vMeas()
			    				   .execute();
			}
		} else {
			if (_port.empty()) {
                string pinType = PinUtility.getPinType(_pin.c_str());

				if (pinType == "DCS-DPS128HC" || pinType == "DCS-DPS128HV") {  // Measure using DC instrument
					rdi.dc().pin("VDD_MRAM0_WL").vForce(0 V).execute();  // discharge the caps
					rdi.dc().pin("VDD_MRAM0_WL").disconnect().execute(); // disconnect the DpS

					rdi.dc(suiteName).pin("VDD_MRAM0_WL").iForce(_forceValue).measWait(_settlingTime).vMeas().execute(); // measure using DC instrument
					cout << "Measure VDD_MRAM0_WL using DPS" << endl;

				} else if (pinType == "PS1600"){   // measure using PPMU on channel (measuring 2.35V which is the applied DC voltage)

					if (_pin == "VDD_MRAM0_WL_CH") {
						rdi.dc().pin("VDD_MRAM0_WL").vForce(0 V).execute();  // discharge the caps
						rdi.dc().pin("VDD_MRAM0_WL").disconnect().execute(); // disconnect the DpS
						cout << "Measure VDD_MRAM0_WL using PPMU on IO" << endl;
					}
					rdi.wait(5 ms);  // allow for additional discharge if needed (we tried up to 5s still not able)

					SMART_RDI::dcBase & prdi = rdi.dc(suiteName)
                                        		  .pin(_pin)
                                        		  .clamp(_clampLo, _clampHi)
                                        		  .iForce(_forceValue)
                                        		  .vRange(4 V)
                                        		  .measWait(_settlingTime)
                                        		  .relay(TA::ppmuRly_onPPMU_offACDC,TA::ppmuRly_onAC_offDCPPMU)
                                        		  .vMeas();

					filterRDI(prdi);
					prdi.execute();
					if (_pin == "VDD_MRAM0_WL_CH") {
						rdi.dc().pin("VDD_MRAM0_WL").connect().execute();
					}
				}
			} else {
				SMART_RDI::dcBase & prdi = rdi.port(_port).dc(suiteName)
                                        		.pin(_pin)
                                                .clamp(_clampLo, _clampHi)
                                        		.iForce(_forceValue)
                                        		.measWait(_settlingTime)
                                        		.relay(TA::ppmuRly_onPPMU_offACDC,TA::ppmuRly_onAC_offDCPPMU)
                                        		.vMeas();
				filterRDI(prdi);
				prdi.execute();
			}


		}


	} else {

		if (_port.empty()) {
			SMART_RDI::dcBase & prdi = rdi.dc(suiteName)
                        		.pin(_pin)
                        		.vForce(_forceValue)
                        		.relay(TA::ppmuRly_onPPMU_offACDC,TA::ppmuRly_onAC_offDCPPMU)
                        		.measWait(_settlingTime)
                        		.iRange(_iRange)
                        		.iMeas();

			filterRDI(prdi);
			prdi.execute();	      
		} else {
			SMART_RDI::dcBase & prdi = rdi.port(_port).dc(suiteName)
                        		.pin(_pin)
                        		.vForce(_forceValue)
                        		.relay(TA::ppmuRly_onPPMU_offACDC,TA::ppmuRly_onAC_offDCPPMU)
                        		.measWait(_settlingTime)
                        		.iRange(_iRange)
                        		.iMeas();


			filterRDI(prdi);
			prdi.execute();	      
		}
	}

	if (_applyShutdown) {
		if (_port.empty()) {
			rdi.func(suiteName + "f2").label(_shutdownPattern).execute();
		} else {
			rdi.port(_port).func(suiteName + "f2").burst(_shutdownPattern).execute();
		}
	}

	RDI_END();

	FOR_EACH_SITE_BEGIN();
	site = CURRENT_SITE_NUMBER();
	funcResultsPre[site] = rdi.id(suiteName + "f1").getPassFail();
	if (_applyShutdown) funcResultsPost[site] = rdi.id(suiteName + "f2").getPassFail();
	// TODO: This retrieval needs to move to the SMC func in the async case
	if (offline()) {
		double dHigh(0), dLow(0);
		TM::COMPARE cHigh, cLow;
		testLimits().TEST_API_LIMIT.get(cLow, dLow, cHigh, dHigh);

		if (cLow != TM::NA && cHigh != TM::NA) {
			results[site] = ((dHigh - dLow) / 2) + dLow;
		} else if (cLow != TM::NA) {
			results[site] = dLow;
		} else if (cHigh != TM::NA) {
			results[site] = dHigh;
		} else {
			results[site] = 0;
		}

	} else {
		results[site] = rdi.id(suiteName).getValue();
	}

	FOR_EACH_SITE_END();

	ON_FIRST_INVOCATION_END();
}

void DCMeasurement::serialProcessing(int site) {
	if (_processResults) {
		judgeAndDatalog(testName() + "_FUNCPRE", invertFunctionalResultIfRequired(funcResultsPre[site]));

		judgeAndDatalog(testName(), filterResult(results[site]));

		if (_applyShutdown && _checkShutdown) {
			judgeAndDatalog(testName() + "_FUNCPOST", invertFunctionalResultIfRequired(funcResultsPost[site]));
		}
	}
}

void DCMeasurement::SMC_backgroundProcessing() {
	for (int i = 0; i < activeSites.size(); i++) {
		int site = activeSites[i];
		process(site);
		if (_processResults) {
			SMC_TEST(site, "", suiteName, LIMIT(TM::GE, 1, TM::LE, 1), funcResultsPre[site]);
			if (_applyShutdown && _checkShutdown) SMC_TEST(site, "", suiteName, LIMIT(TM::GE, 1, TM::LE, 1), funcResultsPost[site]);
			SMC_TEST(site, _pin, suiteName, testLimits().TEST_API_LIMIT, filterResult(results[site]));
		}
	}
}

}
}
