/*'******************************************************************
'*  Name    : GRBL universal DRO (grudr11)                          *
'*  Author  : jpbbricole                                            *
'*  Date    : 28.11.2016                                            *
'*  Version : projectVersion                                        *
'*  Notes   : Requests (?) or receives automatically (UGS)          *
'*            status line from GRBL Since 1.1 (S11)
'*                                                                  *
'* This version needs Arduino with multiple hardware serial ports   *
'* Arduino Leonardo or greater.                                     *
'*            After treatment status data are                       *
'*            sent to a LCD i2C Display                             *
'*            and optionally to a DRO (Excel sheet in this example) *
'*                                                                  *
'*            you can send commands to the program,                 *
'*            see the Monitor subroutines.                          *
'* ---------------------- GRBL since 1.1 (S11) -------------------- *
'* GRBL Version Grbl 1.1 and 115200 and UGS 2.0                     *
'* Setting GRBL:                                                    *
*' $10=1 it displays Feed and Speed                                 *
*' $10=2 it displays Buffer, Feed and Speed                         *
'* --------------------- GRBL documentation ----------------------- *
'* https://github.com/gnea/grbl/blob/edge/doc/markdown/settings.md  *
'* https://github.com/gnea/grbl/blob/edge/doc/markdown/interface.md *
'********************************************************************
*/
String projectName = "GRUDR11";
String projectVersion = "0.5";

#include <Wire.h>                                          // Comes with Arduino IDE
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,20, 4);

#define  grblFormatDecimalNumbers 2
enum grblS11StatusValuesIndex {grblS11ValStatus, grblS11MachX, grblS11MachY, grblS11MachZ, grblS11BufBlocks, grblS11BufBytes, grblS11Feed, grblS11Spindle, 
                               grblS11WorkX, grblS11WorkY, grblS11WorkZ, grblS11ValLasted};
const int grblStatusValuesDim = grblS11ValLasted;
String grblStatusValues[grblS11ValLasted];

enum grblS11AxisValFloatIndex {grblS11axisX, grblS11axisY, grblS11axisZ, grblS11axisLasted};
int grblS11AxisValIntMpos[grblS11axisLasted];
int grblS11AxisValIntWpos[grblS11axisLasted];

enum grblS11WcoValuesIndex {grblS11WcoX, grblS11WcoY, grblS11WcoZ, grblS11WcoLasted};
int grblS11WcoValInt[grblS11WcoLasted];                    // WCO floating values

int grblS11PosMode;                                         // Working Pos WPos  or  Machine Pos MPos
String grblS11PosModeStr;
enum grblS11PosModeIndex {grblS11posMach, grblS11posWork, grblS11posLasted};
    
String grblStatusPrevious = "";                            // thes variables to avoid sending values that do not change
boolean grblStatusTimeToDisplay;                           // If time to display or send GRBL status to display or DRO
                                                           // To avoid clutter in the rapid arrival of status (UGS)

String grblStatusExtraPrevious = "";                       // Contain WCO: 0v: and other extra data                           
String grblStatusExtra;
String grblStatusExtraWco;

unsigned long grblPollingTimer = millis();
int grblPollingPeriod = 200;                               // For every 250 mS

unsigned long grblActivityWatchdog = millis();             // Timer for watching GRBL data line activity
int grblActivityWatchdogPeriod;

#define grblTxSwitch 7                                     // Electronic GRBL Tx switch command
boolean grblStringAvailable = false;                       // If a complete string received from GRBL
String  grblStringRx = "";                                 // Received string
int grblVersion = 0;
enum grblVersionsIndex {grblVersionUnknown, grblVersionSince11, grblVersionLasted};
//     Internal Parameters
boolean interParModeAuto = true;                           // Function mode program true(default): the program waits GRBL's data
                                                           // if you send a Gcode file, it is imperative to be interParModeAuto = true
                                                           // otherwise the polling might create transmission errors.
boolean interParDroConnected = true;                       // If a DRO connected (internal parameter false default)
String interParDroIdentity = "DROX";
//     Monitor Parameters
bool monCommandNew = false;                                // If a new command received from IDE Arduino monitoe
String monCommand = "";                                    // received command
//     Tools Parameters
#define  SplittingArrayDim grblS11ValLasted
String   SplittingArray[SplittingArrayDim];

boolean grudrDebugOn = false;

void setup()
{
    Serial.begin(115200);                                  // IDE monitor or DRO serial
    Serial1.begin(115200);                                 // GRBL Communication
    lcd.begin();
    lcd.noBacklight();

    pinMode(grblTxSwitch, OUTPUT);
    digitalWrite(grblTxSwitch, LOW);
    
    grblStatusTimeToDisplay = false;
    
    grblActivityWatchdogPeriod = grblPollingPeriod-50;

    delay(500);
    lcd.backlight();
    Serial.println(projectName + " " + projectVersion);
    
    grblStatusDisplay();
    lcdPrint(projectName + " " + projectVersion, 0, 0);
}

void loop()
{
    if (millis() - grblPollingTimer > grblPollingPeriod)   // GRBL status timout
    {
        if (!interParModeAuto){grblStatusRequest();}
        grblStatusTimeToDisplay = true;
        
        // grblPollingTimerRestart();
        grblPollingTimer = millis();
    }
    
    serial1Event();                                        // GRBL communications
    if (grblStringAvailable)
    {
        grblStringAvailable = false;
        grblRxEvaluation(grblStringRx);
        grblStringRx = "";
        // grblActivityTimerRestart();
        grblActivityWatchdog = millis();
    }

    serialEvent();
    if (monCommandNew)                                          // Arduino IDE monitor
    {
        monCommTreatment(monCommand);
        monCommand = "";
        monCommandNew  = false;
    }
}
//------------------------------------ GRBL subroutines
void grblStatusRequest()
{
    if (millis() - grblActivityWatchdog > grblActivityWatchdogPeriod)
    {
        Serial.println("Status ?");
        grblCmdSend("?");
    }
}

void grblCmdSend(String txCmd)
{
    if (!interParModeAuto)
    {
        digitalWrite(grblTxSwitch, HIGH);
        delay(50);
        Serial1.print(txCmd + '\r');
        delay(50);
        digitalWrite(grblTxSwitch, LOW);
    }
    else
    {
        Serial.println("Mode auto! (" + txCmd+ ")") ;
    }
}

void grblRxEvaluation(String grblRx)
{
    if (grblRx.startsWith("<"))                                                // Status GRBL
    {
        int startStatPos = grblRx.indexOf("<");                                // Cleans the status line.
        int endStatPos = grblRx.indexOf(">");
        grblRx = grblRx.substring(startStatPos+1, endStatPos-0);

        if (grblRx.indexOf("|") >= 0)                                          // GRBL version Since version 1.1 (S11)
        {
            grblVersion = grblVersionSince11;
            grblStatusRxS11(grblRx);
        }
        else                                                                   
        {
            grblVersion = grblVersionUnknown;
        }
    }
    else if (grblRx == "ok")
    {
        
    }
    else
    {
        Serial.println("!!! GRBL " + grblRx);
    }
}
/*------------------------------------------------------------------------------
    The status line can take these various states:
    $10=1
    <Idle|MPos:0.000,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000>
    <Idle|MPos:0.000,0.000,0.000|FS:0,0|Ov:100,100,100>
    <Idle|MPos:0.000,0.000,0.000|FS:0,0>
    $10=2
    <Idle|WPos:0.000,0.000,0.000|Bf:15,128|FS:0,0|WCO:0.000,0.000,0.000>
    <Idle|WPos:0.000,0.000,0.000|Bf:15,128|FS:0,0|Ov:100,100,100>
    <Idle|WPos:0.000,0.000,0.000|Bf:15,128|FS:0,0>
--------------------------------------------------------------------------------
*/
void grblStatusRxS11(String grblStat)
{
    static boolean statNew;
    static byte statNoNew;
    String grblStatOrigin = grblStat;                                     // For Status values array
    grblStat.toUpperCase();

    /*-----------------------------------------------------------
        Separate data after FS:, Ov: WCO: Pn: etc.(see GRBL doc)
        as StatusExtra for treatment
    '*-----------------------------------------------------------
    */
    int fsPos = grblStat.indexOf("|F");                                   // Search the end of status standard status data
    fsPos = grblStat.indexOf("|", fsPos+2);
    
    grblStatusExtra = "";
    if (fsPos >= 0)
    {
        grblStatusExtra = grblStat.substring(fsPos+1);                    // Extract the extra status data
    }
    grblStat = grblStat.substring(0, fsPos);                              // Delete extra data from status line

    grblS11PosMode = grblS11posMach;
    if(grblStat.indexOf("WPOS") >=0){grblS11PosMode = grblS11posWork;}    // Find if a Mpos or a Wpos status line
            
    grblStat.replace("BF:","");                                           // clean up unnecessary data
    grblStat.replace("|",",");
    grblStat.replace("MPOS:","");                                          
    grblStat.replace("WPOS:","");
    grblStat.replace("FS:","");

    if (grudrDebugOn){Serial.println(grblStatOrigin); Serial.println(grblStat);}

    if (grblStat != grblStatusPrevious)
    {
        grblStatusPrevious = grblStat;
        statNew = true;
        statNoNew = 0;
    }
    else
    {
        statNew = false;
        statNoNew ++;
        if (statNoNew > 10)                                     // For if there is no new status, refresh display
        {
            statNoNew = 0;
            statNew = true;
        }
    }
    toSplitCommand(grblStat, ",");
    
    for(int i = 0; i < grblStatusValuesDim; i++)
    {
        grblStatusValues[i] = SplittingArray[i];
    }
    // Fill Mpos and Wpos arrays with the same received values
    grblS11AxisValIntMpos[grblS11axisX] = grblValuesStrFloatToInt(grblStatusValues[grblS11MachX]);           // It is easier to work integers than floating values
    grblS11AxisValIntMpos[grblS11axisY] = grblValuesStrFloatToInt(grblStatusValues[grblS11MachY]);           // So I work with 1/100 mm
    grblS11AxisValIntMpos[grblS11axisZ] = grblValuesStrFloatToInt(grblStatusValues[grblS11MachZ]);           // I get this -0.200,3.010,10.000 from GRBL 
                                                                                                             // I put this -20,301,1000 in array
    grblS11AxisValIntWpos[grblS11axisX] = grblS11AxisValIntMpos[grblS11axisX];
    grblS11AxisValIntWpos[grblS11axisY] = grblS11AxisValIntMpos[grblS11axisY];
    grblS11AxisValIntWpos[grblS11axisZ] = grblS11AxisValIntMpos[grblS11axisZ];

    if (grblStatusExtra != "")
    {
        if (grblStatusExtra.startsWith("WCO:"))
        {
            grblStatusExtraWco = grblStatusExtra;
        }

        if (grudrDebugOn){Serial.println("Extra: " + grblStatusExtra);}
    }
    grblStatusWcoTreatment(grblStatusExtraWco);                      // Add WCO values to Mpos or Wpos values

    if (grblStatusTimeToDisplay && statNew)
    {
        Serial.println("Stat: " + grblStat);
        grblStatusDisplay();
        if (interParDroConnected) 
        {
            droStatusGrblU11send();
            droStatusOtherSend();
        }
        grblStatusTimeToDisplay = false;
    }
}

void grblStatusWcoTreatment(String wcoValues)                        // WCO:0.000,0.000,0.000
{
    wcoValues.replace("WCO:", "");

    toSplitCommand(wcoValues, ",");
    
    for(int i = 0; i < grblS11WcoLasted; i++)
    {
        grblS11WcoValInt[i] = grblValuesStrFloatToInt(SplittingArray[i]); 
    }

    switch (grblS11PosMode)
    {
        case grblS11posMach:                                                        // If Machine positions received create working position
            grblS11AxisValIntWpos[grblS11axisX] -= grblS11WcoValInt[grblS11WcoX]; 
            grblS11AxisValIntWpos[grblS11axisY] -= grblS11WcoValInt[grblS11WcoY];
            grblS11AxisValIntWpos[grblS11axisZ] -= grblS11WcoValInt[grblS11WcoZ];
            // Moves Speed and Spindle to their array position.
            grblStatusValues[grblS11Feed] = grblStatusValues[grblS11BufBlocks];
            grblStatusValues[grblS11Spindle] = grblStatusValues[grblS11BufBytes];
            grblStatusValues[grblS11BufBlocks] = "-";
            grblStatusValues[grblS11BufBytes] = "-";
            break;
        case grblS11posWork:                                                        // If Machine positions received creat working position
            grblS11AxisValIntMpos[grblS11axisX] += grblS11WcoValInt[grblS11WcoX];
            grblS11AxisValIntMpos[grblS11axisY] += grblS11WcoValInt[grblS11WcoY];
            grblS11AxisValIntMpos[grblS11axisZ] += grblS11WcoValInt[grblS11WcoZ];
            break;
    }
}

int grblValuesStrFloatToInt(String strFloatValue)
{
    return (strFloatValue.toFloat())*pow(10, grblFormatDecimalNumbers);
}

String grblValuesIntToStrFloat(int IntValue)
{
    float retVal = (float)IntValue / pow(10, grblFormatDecimalNumbers);
    return (String)retVal;
}

void grblStatusDisplay()
{
    static int actualVersion;
    
    if (grblVersion != actualVersion)
    {
        lcdCls();
        actualVersion = grblVersion;
    }
    
    switch (grblVersion)
    {
        case grblVersionSince11:
            grblStatusDisplayS11();
            break;
        default:
            lcdCls();
            lcdPrint("Waiting data", 0, 2);
            break;
    }   
}

void grblStatusDisplayS11()
{
    static int actualPosMode;
    
    if (grblS11PosMode != actualPosMode)
    {
        lcdCls();
        actualPosMode = grblS11PosMode;
    }
    
    switch (grblS11PosMode)
    {
        case grblS11posMach:
            grblS11PosModeStr = "m";
            lcdPrintLength("F" + grblStatusValues[grblS11Feed], 6, 0, 5);
            lcdPrintLength("S" + grblStatusValues[grblS11Spindle], 12, 0, 7);
            break;
        case grblS11posWork:
            grblS11PosModeStr = "w";
            lcdPrintLength("B" + grblStatusValues[grblS11BufBlocks], 6, 0, 3);
            lcdPrintLength("F" + grblStatusValues[grblS11Feed], 10, 0, 4);
            lcdPrintLength("S" + grblStatusValues[grblS11Spindle], 15, 0, 5);
            break;
        default:
            grblS11PosModeStr = "? ";
            break;
    }
    lcdPrintLength(grblStatusValues[grblS11ValStatus], 0,0, 6);
    lcdPrintLength(grblS11PosModeStr, 19,3, 1);

    lcdPrintLength("Xwm " + grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisX]), 0,1, 11); lcdPrintLength(grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisX]), 12, 1, 8);
    lcdPrintLength("Ywm " + grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisY]), 0,2, 11); lcdPrintLength(grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisY]), 12, 2, 8);
    lcdPrintLength("Zwm " + grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisZ]), 0,3, 11); lcdPrintLength(grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisZ]), 12, 3, 7);
}

void droStatusGrblU11send()
{
    //---------------------------- GRBL U11 Status
    droSendValues("GSTAT",1,grblStatusValues[grblS11ValStatus]);
    droSendValues("GBUFF",1,grblStatusValues[grblS11BufBlocks]);
    droSendValues("GFEED",1,grblStatusValues[grblS11Feed]);
    droSendValues("GSPIN",1,grblStatusValues[grblS11Spindle]);
    //---------------------------- Machine positions
    droSendValues("GMX",1,grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisX]));
    droSendValues("GMY",1,grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisY]));
    droSendValues("GMZ",1,grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisZ]));
    //---------------------------- Working positions
    droSendValues("GWX",1,grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisX]));
    droSendValues("GWY",1,grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisY]));
    droSendValues("GWZ",1,grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisZ]));

    droSendValues("GWCO",1,grblValuesIntToStrFloat(grblS11WcoValInt[grblS11WcoX]) + " " + grblValuesIntToStrFloat(grblS11WcoValInt[grblS11WcoY]) + " " + 
                                 grblValuesIntToStrFloat(grblS11WcoValInt[grblS11WcoZ]));
    droSendValues("GMODE",1,grblS11PosModeStr);
}

void droStatusOtherSend()
{
    String modeAuto = "On";
    if (!interParModeAuto) {modeAuto = "Off";}
    droSendValues("GPAUTO",1,modeAuto);
}

void droSendValues(String droName, int droIndex, String droValue)
{
    if (interParDroConnected)
    {
        String droCommand = "";

        droCommand = interParDroIdentity + "," + droName + "," + String(droIndex) + "," + droValue + "\n";
        Serial.print(droCommand);
    }
}
/*------------------------------------ Monitor subroutines ------------------------------------
' You can send commands to the program by IDE monitor command line or, if connected, by the DRO
' for internal parameters or, in transit, for GRBL
'
' commRx: GRBL/xxxxx/ for GRBL command
'         INTERNAL/xxxxxx for internal command
'
' GRBL/G91 G0 X12 Y12 Z5
' GRBL/G90 G0 X0 Y0 z0
' INTERNAL/DROconnected/0     INTERNAL/GRBLPOLLPER/250
' INTERNAL/MODEAUTO/0
'----------------------------------------------------------------------------------------------
*/
void monCommTreatment(String commRx)
{
    commRx.toUpperCase();
    toSplitCommand(commRx, "/");                           // To put received parameters in array
    String destDevice = SplittingArray[0];                 // Destination device
    if (destDevice == "GRBL")
    {
        grblCmdSend(SplittingArray[1]);
    }
    else if (destDevice == "INTERNAL")                          // INTERNAL/POLLstat/0
    {
        interCmdExecute(SplittingArray[1], SplittingArray[2]);
    }
    else
    {
        Serial.println("Unknown dest. device!! " + destDevice);
    }
}
//------------------------------------ Internal subroutines
void interCmdExecute(String intCmdFct, String intCmdPar1)
{
    int cmdPar1Val = intCmdPar1.toInt();
    if (intCmdFct  == "DROCONNECTED")                           // Connect or disconnect DRO
    {
        if (cmdPar1Val == 1) {interParDroConnected = true;} else {interParDroConnected = false;}
    }
    else if (intCmdFct  == "GRBLPOLLPER")
    {
        grblPollingPeriod = cmdPar1Val;
    }
    else if (intCmdFct  == "MODEAUTO")
    {
        if (cmdPar1Val == 1) {interParModeAuto = true;} else {interParModeAuto = false;}
    }
    else
    {
        Serial.println("Internal cmd " + intCmdFct + " !!");
    }
}

//------------------------------------ Communication subroutines
void serial1Event()                                              // GRBL serial
{
    while (Serial1.available())
    {
        char MinChar = (char)Serial1.read();                    // Char received from GRBL
        if (MinChar == '\n')
        {
            grblStringAvailable = true;
        }
        else
        {
            if (MinChar >= ' ') {grblStringRx += MinChar;}      // >= ' ' to avoid not wanted ctrl char.
        }
    }
}

void serialEvent()                                             // IDE monitor or DRO serial
{
    while (Serial.available())
    {
        char monChar = (char)Serial.read();                     // Char received from IDE monitor
        if (monChar == '\n')                                    // If new line char received = end of command line
        {
            monCommandNew  = true;
        }
        else
        {
            if (monChar >= ' ') {monCommand += monChar;}        // >= ' ' to avoid not wanted ctrl char.
        }
    }
}
//---------------------------------------------- Tools section
/*-----------------------------------------------------------------------------------------------------------------------------
*' toSplitCommand for to split received string delimited with expected char separator par1, par2, par3, .....
*' and put them in an array (SplittingArray)
*'-----------------------------------------------------------------------------------------------------------------------------
*/
void toSplitCommand(String SplitText, String SplitChar) {
    SplitText = SplitChar + SplitText + SplitChar;
    int SplitIndex = -1;
    int SplitIndex2;

    for(int i = 0; i < SplittingArrayDim - 1; i++) {
        SplitIndex = SplitText.indexOf(SplitChar, SplitIndex + 1);
        SplitIndex2 = SplitText.indexOf(SplitChar, SplitIndex + 1);
        if (SplitIndex < 0 || SplitIndex2 < 0){break;}
        if(SplitIndex2 < 0) SplitIndex2 = SplitText.length() ;
        SplittingArray[i] = SplitText.substring(SplitIndex+1, SplitIndex2);
    }
}
//////////////////////////////////////////////////////////////////////////////////
String lcdEmptyLine = "                    ";
void lcdClsRow(int rowNum)
{
    lcdPrint(lcdEmptyLine, 0, rowNum);
}
void lcdCls()
{
    lcd.clear();
}

void lcdPrint(String lcdText, int lcdCol,int lcdRow)
{
    lcd.setCursor(lcdCol,lcdRow);
    lcd.print(lcdText);
}
void lcdPrintLength(String lcdText, int lcdCol,int lcdRow, int textLength)
{
    String textToLength = lcdText + lcdEmptyLine;
    textToLength = textToLength.substring(0,textLength);
    
    lcdPrint(textToLength, lcdCol, lcdRow);
}
