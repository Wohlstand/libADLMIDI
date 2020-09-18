#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>

LONG testDriver()
{
    HDRVR hdrvr;
    DRVCONFIGINFO dci;
    LONG lRes;

    printf("Open...\n");
    // Open the driver (no additional parameters needed this time).
    if((hdrvr = OpenDriver(L"adlmididrv.dll", 0, 0)) == 0)
    {
        printf("!!! Can't open the driver\n");
        return -1;
    }

    printf("Send DRV_QUERYCONFIGURE...\n");
    // Make sure driver has a configuration dialog box.
    if(SendDriverMessage(hdrvr, DRV_QUERYCONFIGURE, 0, 0) != 0)
    {
        // Set the DRVCONFIGINFO structure and send the message
        dci.dwDCISize = sizeof (dci);
        dci.lpszDCISectionName = (LPWSTR)0;
        dci.lpszDCIAliasName = (LPWSTR)0;
        printf("Send DRV_CONFIGURE...\n");
        lRes = SendDriverMessage(hdrvr, DRV_CONFIGURE, 0, (LONG)&dci);
        printf("<-- Got answer: %ld)\n", lRes);
    }
    else
    {
        printf("<-- No configure\n");
        lRes = DRVCNF_OK;
    }

    printf("Close...\n");
    // Close the driver (no additional parameters needed this time).
    if(FAILED(CloseDriver(hdrvr, 0, 0)))
    {
        printf("!!! Error when closing\n");
        return -1;
    }

    printf("Return...\n");

    return 0;
}

int main()
{
    LONG d = testDriver();
    if(d == 0)
    {
        printf("TEST = OK\n");
    }
    else
    {
        printf("TEST = FAILED\n");
    }
    return 0;
}
