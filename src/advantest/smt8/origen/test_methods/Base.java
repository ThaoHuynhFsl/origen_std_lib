package origen.test_methods;

import origen.common.Origen;
import origen.common.OrigenDeviceData;
import xoc.dta.ITestContext;
import xoc.dta.TestMethod;
import xoc.dta.datatypes.MultiSiteBoolean;
import xoc.dta.datatypes.MultiSiteDouble;
import xoc.dta.datatypes.MultiSiteLong;
import xoc.dta.measurement.IMeasurement;
import xoc.dta.resultaccess.IMeasurementResult;
import xoc.dta.testdescriptor.IFunctionalTestDescriptor;
import xoc.dta.testdescriptor.IParametricTestDescriptor;

/** Origen testmethods base class. All testmethods inherit from this class */
public class Base extends TestMethod {
  /** General testmethod parameters, used by all testmethods */
  public IMeasurement measurement;

  public String testName;
  // TODO Verify forcePass implementation (also check if implemented in DC_Measurements)
  public Boolean forcePass = false;
  // Sites that passed will contain a '1' if forcePass has been set, otherwise undefined
  public MultiSiteLong setOnPassFlags;
  // Sites that failed will contain a '1' if forcePass has been set, otherwise undefined
  public MultiSiteLong setOnFailFlags;
  // When set to true the tester will never be released by Origen code, though your application
  // test method code is still free to do so if you want
  public Boolean sync_par = false;
  /**
   * The log level that will be used during the execution of the TP. Change the value here to get
   * more, or less logging info
   */
  int origenLoglevel = Origen.LOG_WARNING;

  /** Local instance of DeviceData storage */
  OrigenDeviceData devDataStorage = null;

  /**
   * Keep track if release93k has been called. If background not allowed this does not
   * automatically mean we are in background thread
   */
  private boolean release93kCalled = false;

  /**
   * Execute the checkparms() function? This is used to check if all testmethod parameters are
   * parsed
   */
  boolean checkParams = true;

  public void logTrace(String className, String method) {
    message(Origen.LOG_METHODTRACE, "\t" + className + "\t" + method + "()");
  }

  // ********* FLOW LOGIC BELOW **********

  /**
   * Overridden setup function from the testmethod
   *
   * <p>The order is that first the setup() function is called by SMT, then the update() function
   * and then the execute() function
   *
   * <p>This setup() function calls the _setup function from the underlying class (Can be
   * DC_Measurement or Function_test), depending on the type of TM These subclasses are responsible
   * for proper handling of the TM setup.
   */
  @Override
  public void setup() {
    Origen.context = context;
    Origen.meas = measurement;
    messageLogLevel = origenLoglevel;
    logTrace("Base", "setup");
    if (!dependenciesUnchanged()) {
      _setup();
    }
  }

  /**
   * Overridden update function from the testmethod
   *
   * <p>The order is that first the setup() function is called by SMT, then the update() function
   * and then the execute() function
   *
   * <p>The update() function does nothing at the moment. It acts as a placeholder because all TM's
   * should have an update function. If a TM wants the use the update() functionality from SMT it
   * can implement this function itself
   */
  @Override
  public void update() {
    logTrace("Base", "update");
  }

  /**
   * Overridden execute function from the testmethod
   *
   * <p>The order is that first the setup() function is called by SMT, then the update() function
   * and then the execute() function
   *
   * <p>The execute() function first checks if the parameters are parsed. It then calls the body()
   * function from the testmethod. This is the main function of the TM. Finally it necessary, it
   * calls the process() function from the TM.
   */
  @Override
  public void execute() {
    logTrace("Base", "execute");
    release93kCalled = false;

    if (checkParams) {
      checkParams();
    }

    if (forcePass) {
      setOnPassFlags = new MultiSiteLong(1);
      setOnFailFlags = new MultiSiteLong(0);
    }

    // Call the internal pre body function
    _preBody();

    // Call the application test method body
    body();

    // Call the application test method process method
    process();

    // Call the internal process results method
    processResults();

    releaseVariables();

  }

  /**
   * Placeholder for the checkParams function. The goal of this function is to check if all the
   * testmethod parameters that are passed to a testmethod are actually used This will make sure
   * that parameters are not ignored. Should be overridden by testmethods when needed
   */
  public void checkParams() {
    logTrace("Base", "checkParams");
  }

  public void measure_setup() {
    logTrace("Base", "measure_setup");
  }

  public void process() {
    logTrace("Base", "process");
  }

  public void _setup() {
    logTrace("Base", "_setup");
  }

  public void run() {
    logTrace("Base", "run");
  }

  public void _preBody() {
    logTrace("Base", "_preBody");
  }

  public void body() {
    logTrace("Base", "body");
    run();
  }

  public void processResults() {
    logTrace("Base", "processResults");
  }

  public void judgeAndDatalog(IFunctionalTestDescriptor t, MultiSiteBoolean passed) {
    MultiSiteBoolean allPassed = new MultiSiteBoolean(true);

    if (forcePass) {
      for (int site : context.getActiveSites()) {
        setOnPassFlags.set(site, setOnPassFlags.get(site) & (passed.get(site) ? 1 : 0));
        setOnFailFlags.set(site, setOnFailFlags.get(site) | (passed.get(site) ? 0 : 1));
      }
      // Record that this test happened to STDF, but don't know how to log the true result
      // without also causing it to fail/bin
      t.evaluate(allPassed);
    } else {
      t.evaluate(passed);
    }
    for (int site : context.getActiveSites()) {
      message(
          Origen.LOG_PARAM,
          "["
              + site
              + "]("
              + t.getTestName()
              + ") "
              + " : "
              + (passed.get(site) ? "PASSED" : "FAILED"));
    }
  }

  public void judgeAndDatalog(IFunctionalTestDescriptor t, IMeasurementResult measurementResult) {
    MultiSiteBoolean passed = measurementResult.hasPassed();
    judgeAndDatalog(t, passed);
  }

  /**
   * Log a multisite double
   *
   * @param t Name of the testdescriptor
   * @param MSD
   */
  public void judgeAndDatalog(IParametricTestDescriptor t, MultiSiteDouble MSD) {

    boolean loLimitPresent = false, hiLimitPresent = false;
    double lo = 0;
    double hi = 0;
    if (forcePass) {
      if (t.getLowLimit() == null) {
        loLimitPresent = false;
      } else {
        loLimitPresent = true;
        lo = t.getLowLimit().doubleValue();
      }
      if (t.getHighLimit() == null) {
        hiLimitPresent = false;
      } else {
        hiLimitPresent = true;
        hi = t.getHighLimit().doubleValue();
      }

      for (int site : context.getActiveSites()) {
        boolean passed = true;
        double val = MSD.get(site);

        // TODO: How to handle difference between LT and LTE?
        if (loLimitPresent) {
          if (val < lo) {
            passed = false;
          }
        }

        if (hiLimitPresent) {
          if (val > hi) {
            passed = false;
          }
        }

        setOnPassFlags.set(site, setOnPassFlags.get(site) & (passed ? 1 : 0));
        setOnFailFlags.set(site, setOnFailFlags.get(site) | (passed ? 0 : 1));
      }

      t.setLowLimit(Double.NaN);
      t.setHighLimit(Double.NaN);
    }

    t.evaluate(MSD);

    if (forcePass) {
        if(loLimitPresent) {
            t.setLowLimit(lo);
        }
        if(hiLimitPresent) {
            t.setHighLimit(hi);
        }
    }

    MultiSiteBoolean pf = t.getPassFail();
    for (int site : context.getActiveSites()) {
      message(
          Origen.LOG_PARAM,
          "["
              + site
              + "]("
              + t.getTestName()
              + ") "
              + MSD.get(site)
              + " : "
              + (pf.get(site) ? "PASSED" : "FAILED"));
    }
  }

  /**
   * Log a multisite long
   *
   * @param t Name of the testdescriptor
   * @param MSL
   */
  public void judgeAndDatalog(IParametricTestDescriptor t, MultiSiteLong MSL) {
    judgeAndDatalog(t, MSL.toMultiSiteDouble());
  }

  /**
   * From the SMT8_Origen_update.pptx
   * Use same parametric datalogging method as smt7
   * 0 = pass, -1 = fail
   */
  public static MultiSiteLong ftd2Ptd(MultiSiteBoolean results) {
    MultiSiteLong Param = new MultiSiteLong();

    for (int site : results.getActiveSites()) {
      if (results.get(site)) {
        Param.set(site, 0); // test passed
      } else {
        Param.set(site, -1); // test failed
      }
    }
    return Param;
  }

  public void release93k() {
      release93kCalled = true;
      releaseTester();
  }

  public boolean hasRelease93kBeenCalled() {
      return release93kCalled;
  }

  public static MultiSiteLong ftd2Ptd(IMeasurementResult results) {
    return ftd2Ptd(results.hasPassed());
  }

  public void setOrigenDeviceDataStorage(OrigenDeviceData _devData) {
    devDataStorage = _devData;
  }

  /** Release all locked variables */
  private void releaseVariables() {
      if (devDataStorage != null) {
          devDataStorage.releaseVariables();
      }
  }

  public ITestContext getContext() {
    return context;
  }

}
