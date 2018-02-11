#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <vector>
#include <list>
#include "common_funcs.h"

#include <my_global.h>
#include <mysql.h>

typedef struct {
  char    ExpireDate[11];
  char    TranDate[11];
  double  StrikePrice;
  double  StockPrice;
  double  LowStrike;
  double  HighStrike;
} EDATES;

typedef struct {
  char    TranDate[11];
  double  SellPrice;
} ESVALUES;

MYSQL *ConnectToDB();

std::vector<char*>  gsFridayDates;
std::list<char*>    gsSymbols;
MYSQL               *gstDBConn;
char                gsCurrentDate[12];
char                *gsDateList = NULL;
std::list<ESVALUES *>   gdCPrices;
std::list<ESVALUES *>   gdPPrices;
double                  gdCBuyPrice;
double                  gdPBuyPrice;
char                    *lsL1, *lsL2, *lsL3, *lsL4, *lsL5, *lsL6;
char                    *lsL1P, *lsL2P, *lsL3P, *lsL4P, *lsL5P, *lsL6P;
double                  gdStrikeLow;
double                  gdStrikeHigh;
int                     giExpireLowDays;
int                     giExpireHighDays;
double                  gdWorkLowStrike;
double                  gdWorkHighStrike;
char                    gcCallHighLow = 'C';
char                    gcPutHighLow = 'C';
double                  gdAutoSellLow = 0.0;
double                  gdAutoSellHigh = 999999.99;

void LoadFridayDates();
double GetSymbolStockPrice( char *asSymbol, char *asDate );
void GetExpireDate( char *asSymbol, char *asDate, char *asExpireDate );
double GetStrikePrice( char *asSymbol, double adQuote, char *asExpireDate, char *asTranDate );
void PrintResults( char *asSymbol, double adStockQuote, std::list<EDATES *> alEDates );
void PrintResults2( char *asSymbol, double adStockQuote, std::list<EDATES *> alEDates );
void ClearEDateList( std::list<EDATES *> alEDates );
//double GetExpireStrikePrices( char *asSymbol, EDATES *astEDate );
double GetExpireStrikePrices( char *asSymbol, EDATES *astEDate, double adCallStrike, double adPutStrike );
int LoadAllSymbols();
int InitializeParameters( int aiArgc, char **asArgv );

//----------------------------------------------------------------
// Function: main
//----------------------------------------------------------------
int main( int aiArgc, char **asArgv )
{
  int                          liDates;
  std::list<char *>::iterator  lcI;
  double                       ldSymbolQuote;
  char                         lsExpireDate[12];
  double                       ldStrikePrice;
  std::list<EDATES*>           llEDates;
  EDATES                       *lstEDate;

  if( InitializeParameters( aiArgc, asArgv ) )
    return 1;

  GetCurrentDate_YYYYMMDD( (char *)&gsCurrentDate, (char *)"-" );
  LoadFridayDates();
  gstDBConn = ConnectToDB();

//  gsSymbols.push_back( (char *)"efx" );
//  gsSymbols.push_back( (char *)"hd" );
//  gsSymbols.push_back( (char *)"low" );
//  gsSymbols.push_back( (char *)"wmt" );

  if( gsSymbols.size() == 0 )
    LoadAllSymbols();

  lsL1 = (char *)malloc( 4096*6 );
  lsL2 = lsL1+4096;
  lsL3 = lsL2+4096;
  lsL4 = lsL3+4096;
  lsL5 = lsL4+4096;
  lsL6 = lsL5+4096;

  for( lcI = gsSymbols.begin(); lcI != gsSymbols.end(); lcI++ )
  {
    ClearEDateList( llEDates );
    llEDates.clear();
    for( liDates = 0; liDates < gsFridayDates.size(); liDates++ )
    {
      if( strcmp( gsFridayDates[liDates], gsCurrentDate ) > 0 )
        break;

      ldSymbolQuote = GetSymbolStockPrice( *lcI, gsFridayDates[liDates] );
      GetExpireDate( *lcI, gsFridayDates[liDates], (char *)&lsExpireDate );
      ldStrikePrice = GetStrikePrice( *lcI, ldSymbolQuote, lsExpireDate, gsFridayDates[liDates] );
      if( ldStrikePrice != 0.0 && lsExpireDate[0] != 0 )
      {
        lstEDate = (EDATES *)malloc( sizeof( EDATES ) );
        lstEDate->StrikePrice = ldStrikePrice;
        lstEDate->LowStrike = gdWorkLowStrike;
        lstEDate->HighStrike = gdWorkHighStrike;
        lstEDate->StrikePrice = ldStrikePrice;
        lstEDate->StockPrice = ldSymbolQuote;
        strcpy( lstEDate->ExpireDate, lsExpireDate );
        strcpy( lstEDate->TranDate, gsFridayDates[liDates] );
        llEDates.push_back( lstEDate );
      }
//      printf( "  %.2f:%s:%.2f", ldSymbolQuote, lsExpireDate, ldStrikePrice );
    }
//    PrintResults( *lcI, ldSymbolQuote, llEDates );
    PrintResults2( *lcI, ldSymbolQuote, llEDates );

//    printf( "\n" );
  }

  mysql_close( gstDBConn );
  return 0;
}

//----------------------------------------------------------------
// Function: InitializeParameters
//  -sl:  strike low price filter
//  -sh:  strike high price filter
//  -el:  expire days low value
//  -eh:  expire days high value
//  -cl:  Use strike price on low side of closest (CALL).
//  -ch:  Use striek price on high side of closest (CALL).
//  -pl:  Use strike price on low side of closest (PUT).
//  -ph:  Use striek price on high side of closest (PUT).
//----------------------------------------------------------------
int InitializeParameters( int aiArgc, char **asArgv )
{
  int             liIdx;
  time_t          liTime;
  struct tm       *lstTime;
  char            lsDate[11];
  double          ldAutoSell;

  gdStrikeLow = 0.0;
  gdStrikeHigh = 999999.99;
  giExpireLowDays = 80;
  giExpireHighDays = 100;

  for( liIdx = 1; liIdx < aiArgc; liIdx++ )
  {
    if( strcmp( asArgv[liIdx], "-sl" ) == 0 )
    {
      if( liIdx < aiArgc - 1 )
      {
        gdStrikeLow = atof( asArgv[liIdx+1] );
        liIdx++;
      }
      else
      {
        printf( "Missing strike price low value.\n" );
        return 1;
      }
    }
    else if( strcmp( asArgv[liIdx], "-sh" ) == 0 )
    {
      if( liIdx < aiArgc - 1 )
      {
        gdStrikeHigh = atof( asArgv[liIdx+1] );
        liIdx++;
      }
      else
      {
        printf( "Missing strike price low value.\n" );
        return 1;
      }
    }
    else if( strcmp( asArgv[liIdx], "-el" ) == 0 )
    {
      if( liIdx < aiArgc - 1 )
      {
        giExpireLowDays = atoi( asArgv[liIdx+1] );
        liIdx++;
      }
      else
      {
        printf( "Missing expire date low days value.\n" );
        return 1;
      }
    }
    else if( strcmp( asArgv[liIdx], "-eh" ) == 0 )
    {
      if( liIdx < aiArgc - 1 )
      {
        giExpireHighDays = atoi( asArgv[liIdx+1] );
        liIdx++;
      }
      else
      {
        printf( "Missing expire date high days value.\n" );
        return 1;
      }
    }
    else if( strcmp( asArgv[liIdx], "-y" ) == 0 )
    {
      if( liIdx < aiArgc - 1 )
      {
        gsSymbols.push_back( strdup( asArgv[liIdx+1] ) );
        liIdx++;
      }
      else
      {
        printf( "Missing expire date high days value.\n" );
        return 1;
      }
    }
    else if( strcmp( asArgv[liIdx], "-as" ) == 0 )
    {
      if( liIdx < aiArgc - 1 )
      {
        ldAutoSell = atof( asArgv[liIdx+1] );
        if( ldAutoSell < 0.0 )
          gdAutoSellLow = ldAutoSell / 100;
        else
          gdAutoSellHigh = ldAutoSell / 100;
        liIdx++;
      }
      else
      {
        printf( "Missing expire date high days value.\n" );
        return 1;
      }
    }
    else if( strcmp( asArgv[liIdx], "-cl" ) == 0 )
    {
      gcCallHighLow = 'L';
    }
    else if( strcmp( asArgv[liIdx], "-ch" ) == 0 )
    {
      gcCallHighLow = 'H';
    }
    else if( strcmp( asArgv[liIdx], "-pl" ) == 0 )
    {
      gcPutHighLow = 'L';
    }
    else if( strcmp( asArgv[liIdx], "-ph" ) == 0 )
    {
      gcPutHighLow = 'H';
    }
  }

  printf( "Processing options:\n" );
  printf( "  Strike Price Range: {%.2f, %.2f}\n",
          gdStrikeLow, gdStrikeHigh );
  printf( "  Expire Date Range:  {%d, %d}\n",
          giExpireLowDays, giExpireHighDays );
  printf( "  Auto Sell Low / High:  {%.2f/ %.2f}\n",
          gdAutoSellLow, gdAutoSellHigh );
  return 0;
}

//----------------------------------------------------------------
// Function: LoadFridayDates
//----------------------------------------------------------------
void LoadFridayDates()
{
  struct tm  lstTime;
  time_t     liTime;
  struct tm  *lstNewTime;
  int        liIdx;
  char       lsDate[20];
  int        liFridayCnt = 0;
  int        liHoldMonth;
  char       *lsPos;
  int        liLen;

  gsDateList = (char *)malloc( 32*1024 );
  memset( gsDateList, 0, 32*1024 );
  lsPos = gsDateList;

  memset( &lstTime, 0, sizeof( lstTime ) );
  lstTime.tm_year = 117;
  lstTime.tm_mon = 6;
  lstTime.tm_mday = 1;
  lstTime.tm_hour = 10;
  liHoldMonth = lstTime.tm_mon;

  liTime = mktime( &lstTime );

//  printf( "liTime=%d\n", liTime );

  for( liIdx = 0; liIdx < 1800; liIdx++ )
  {
    liTime += 86400;
    lstNewTime = localtime( &liTime );
  
    if( lstNewTime->tm_wday == 5 )
    {
      if( lstNewTime->tm_mon != liHoldMonth )
      {
        liHoldMonth = lstNewTime->tm_mon;
        liFridayCnt = 0;
      }

      liFridayCnt++;

      if( liFridayCnt == 3 )
      {
        sprintf( lsDate, "%d-%02d-%02d",
                 lstNewTime->tm_year + 1900,
                 lstNewTime->tm_mon + 1,
                 lstNewTime->tm_mday );
        gsFridayDates.push_back( strdup( lsDate ) );
        liLen = sprintf( lsPos, "%s'%s'", 
                         lsPos==gsDateList ? "" : ",",
                         lsDate );
        lsPos += liLen;
//        printf( "New Time=%s %02d  Weekday=%d\n",
//                lsDate,
//                lstNewTime->tm_hour,
//                lstNewTime->tm_wday );
      }
    }
  }

//  printf( "gsDateList=%s\n", gsDateList );
}

//----------------------------------------------------------------
// Function: LoadAllSymbols
//----------------------------------------------------------------
int LoadAllSymbols()
{
  double       ldReturn = 0.0;
  char         lsSQL[300];
  MYSQL_RES    *lstResult;
  MYSQL_ROW    lstRow;

  sprintf( lsSQL,
           "select symbol from stock_symbols order by symbol" );

  if( mysql_query( gstDBConn, lsSQL ) )
  {
    printf( "LoadAllSymbols: SELECT failed: %s\n", mysql_error( gstDBConn ) );
    return 0.0;
  }

  lstResult = mysql_store_result( gstDBConn );

  if( lstResult == NULL )
    return 0.0;

  while( ( lstRow = mysql_fetch_row( lstResult ) ) )
  {
    gsSymbols.push_back( strdup( lstRow[0] ) );
  }

  mysql_free_result( lstResult );

  return gsSymbols.size();
}

//----------------------------------------------------------------
// Function: GetSymbolStockPrice
//----------------------------------------------------------------
double GetSymbolStockPrice( char *asSymbol, char *asDate )
{
  double       ldReturn = 0.0;
  char         lsSQL[300];
  MYSQL_RES    *lstResult;
  MYSQL_ROW    lstRow;

  sprintf( lsSQL,
           "select last_price from symbol_daily_quotes where symbol='%s' and tran_date >= '%s' order by tran_date",
           asSymbol,
           asDate );

  if( mysql_query( gstDBConn, lsSQL ) )
  {
    printf( "GetSymbolStockPrice: SELECT failed: %s\n", mysql_error( gstDBConn ) );
    return 0.0;
  }

  lstResult = mysql_store_result( gstDBConn );

  if( lstResult == NULL )
    return 0.0;

  lstRow = mysql_fetch_row( lstResult );

  if( lstRow )
    ldReturn = atof( lstRow[0] );

  mysql_free_result( lstResult );

  return ldReturn;
}

//----------------------------------------------------------------
// Function: GetExpireDate
//----------------------------------------------------------------
void GetExpireDate( char *asSymbol, char *asDate, char *asExpireDate )
{
  char         lsSQL[4000];
  MYSQL_RES    *lstResult;
  MYSQL_ROW    lstRow;
  int          liDays;
  char         *lsLess90Days = NULL;
  char         *ls90Days = NULL;
  char         *lsGreator90Days = NULL;

  asExpireDate[0] = 0;

  sprintf( lsSQL,
           "select distinct expire_date from call_put_strikes where symbol='%s' and expire_date in (%s) and expire_date >= '%s' order by expire_date",
           asSymbol,
           gsDateList,
           asDate );

  if( mysql_query( gstDBConn, lsSQL ) )
  {
    printf( "GetExpireDate: SELECT failed: %s\n", mysql_error( gstDBConn ) );
    return;
  }

  lstResult = mysql_store_result( gstDBConn );

  if( lstResult == NULL )
    return;

  while( ( lstRow = mysql_fetch_row( lstResult ) ) )
  {
    liDays = DateDiffDaysYYYY_MM_DD( lstRow[0], asDate );
    if( liDays < giExpireLowDays )
      lsLess90Days = strdup( lstRow[0] );
    else
      if( liDays > giExpireHighDays )
      {
        if( lsGreator90Days == NULL )
        {
          lsGreator90Days = strdup( lstRow[0] );
          break;
        }
      }
      else
        ls90Days = strdup( lstRow[0] );
//    printf( "  [ %s - %s = %d ]", lstRow[0], asDate, liDays );
  }

//  printf( " [ %s -> %s -> %s -> %s ]",
//          asDate,
//          lsLess90Days==NULL ? "null" : lsLess90Days,
//          ls90Days==NULL ? "null" : ls90Days,
//          lsGreator90Days==NULL ? "null" : lsGreator90Days );

    if( ls90Days )
      strcpy( asExpireDate, ls90Days );
    else
      if( lsLess90Days )
        strcpy( asExpireDate, lsLess90Days );
      else
        if( lsGreator90Days )
          strcpy( asExpireDate, lsGreator90Days );

//  if( lstRow )
//    ldReturn = atof( lstRow[0] );

  mysql_free_result( lstResult );

}

//----------------------------------------------------------------
// Function: GetStrikePrice
//----------------------------------------------------------------
double GetStrikePrice( char *asSymbol, double adQuote, char *asExpireDate, char *asTranDate )
{
  char         lsSQL[4000];
  MYSQL_RES    *lstResult;
  MYSQL_ROW    lstRow;
  int          liDays;
  double       ldLess = 0.0;
  double       ldQuote = 0.0;
  double       ldGreator = 0.0;
  double       ldPrice;
  double       ldReturn = 0.0;

//           "select distinct strike_price from call_put_strikes where symbol='%s' and expire_date = '%s' order by strike_price",
  sprintf( lsSQL,
           "select distinct strike_price "
           "from   call_put_strikes cps, symbol_daily_calls_puts cp "
           "where  cp.call_put_id = cps.call_put_id "
           "and    symbol='%s' "
           "and    tran_date = '%s' "
           "and    expire_date='%s' "
           "order by strike_price",
           asSymbol,
           asTranDate,
           asExpireDate );

  if( mysql_query( gstDBConn, lsSQL ) )
  {
    printf( "GetStrikePrice: SELECT failed: %s\n", mysql_error( gstDBConn ) );
    return 0.0;
  }

  lstResult = mysql_store_result( gstDBConn );

  if( lstResult == NULL )
    return 0.0;

  while( ( lstRow = mysql_fetch_row( lstResult ) ) )
  {
    ldPrice = atof( lstRow[0] );
    if( ldPrice < adQuote )
      ldLess = ldPrice;
    else
      if( ldPrice > adQuote )
      {
        if( ldGreator == 0.0 )
        {
          ldGreator = ldPrice;
          break;
        }
      }
      else
        ldQuote = ldPrice;
  }

    if( ldQuote != 0.0 )
      ldReturn = ldQuote;
    else
    {
      ldPrice = adQuote - ldLess;
      ldReturn = ldGreator - adQuote;
      if( ldPrice < ldReturn )
        ldReturn = ldLess;
      else
        ldReturn = ldGreator;
    }

//  if( lstRow )
//    ldReturn = atof( lstRow[0] );

  mysql_free_result( lstResult );
  gdWorkLowStrike = ldLess;
  gdWorkHighStrike = ldGreator;
  return ldReturn;
}

//----------------------------------------------------------------
// Function: PrintResults
//----------------------------------------------------------------
void PrintResults( char *asSymbol, double adStockQuote, std::list<EDATES *> alEDates )
{
  std::list<EDATES *>::iterator  lcEDate;
  std::list<ESVALUES *>::iterator  lcValue;
  EDATES                         *lstEDate;
  ESVALUES                       *lstValue;
  double                         ldBuyPrice;
  char                           lsPart1[100];
  char                           lsPart2[100];
  double                         ldMin, ldMax;
  double                         ldMinPct, ldMaxPct;
  char                           lsNbr1[20];
  char                           lsNbr2[20];
  char                           lsNbr3[20];
  char                           lsNbr4[20];
  int                            liMinDays, liMaxDays;
  char                           *lsMinDate, *lsMaxDate;
  double                         ldMinValue, ldMaxValue;

  memset( lsL1, 0, 4096*6 );
  lsL1P = lsL1;
  lsL2P = lsL2;
  lsL3P = lsL3;
  lsL4P = lsL4;
  lsL5P = lsL5;
  lsL6P = lsL6;
  lsL1P += sprintf( lsL1, "%12s  ", "" );
  lsL2P += sprintf( lsL2, "%12s  ", "" );
  lsL3P += sprintf( lsL3, "%12s  ", "" );
  lsL4P += sprintf( lsL4, "%-12s  ", asSymbol );
  lsL5P += sprintf( lsL5, "%-12s  ", "" );
  sprintf( lsPart1, "%.2f", adStockQuote );
  lsL6P += sprintf( lsL6, "%-12s  ", lsPart1 );

  for( lcEDate = alEDates.begin(); lcEDate != alEDates.end(); lcEDate++ )
  {
    lstEDate = *lcEDate;
//    printf( "  %.2f:%s:%.2f\n", adStockQuote, lstEDate->ExpireDate, lstEDate->StrikePrice );
    GetExpireStrikePrices( asSymbol, lstEDate, 0.0, 0.0 );

    sprintf( lsPart1, "-------------- %s --------------", lstEDate->ExpireDate );
    liMinDays = sprintf( lsNbr1, " C(%.2f) ", gdCBuyPrice );
    liMaxDays = sprintf( lsNbr2, " P(%.2f) ", gdPBuyPrice );
    memcpy( lsPart1+1, lsNbr1, liMinDays );
    memcpy( lsPart1+(39-liMaxDays), lsNbr2, liMaxDays );
//    sprintf( lsPart1, "%s C(%.2f)", lstEDate->ExpireDate, gdCBuyPrice );
//    sprintf( lsPart2, "%s P(%.2f)", lstEDate->ExpireDate, gdPBuyPrice );
//    lsL1P += sprintf( lsL1P, "%-20s %-20s ", lsPart1, lsPart2 );
    lsL1P += sprintf( lsL1P, "%s  ", lsPart1 );
    lsL2P += sprintf( lsL2P, "      Min       Max        Min       Max  " );
    lsL3P += sprintf( lsL3P, "--------- ---------  --------- ---------  " );

    // Call section...
    ldMin = 99999.99;
    ldMax = -99999.99;
    liMinDays = 0;
    liMaxDays = 0;
    lsMinDate = (char *)"";
    lsMaxDate = (char *)"";
    for( lcValue = gdCPrices.begin(); lcValue != gdCPrices.end(); lcValue++ )
    {
      lstValue = *lcValue;
      if( lstValue->SellPrice < ldMin )
      {
        ldMin = lstValue->SellPrice;
        ldMinPct = ( ( lstValue->SellPrice - gdCBuyPrice ) / gdCBuyPrice ) * 100;
        liMinDays = DateDiffDaysYYYY_MM_DD( lstValue->TranDate, lstEDate->TranDate );
        lsMinDate = lstValue->TranDate;
      }
      if( lstValue->SellPrice > ldMax )
      {
        ldMax = lstValue->SellPrice;
        ldMaxPct = ( ( lstValue->SellPrice - gdCBuyPrice ) / gdCBuyPrice ) * 100;
        liMaxDays = DateDiffDaysYYYY_MM_DD( lstValue->TranDate, lstEDate->TranDate );
        lsMaxDate = lstValue->TranDate;
      }
    }
    sprintf( lsNbr1, "%.2f", ldMin );
    sprintf( lsNbr2, "%.2f", ldMax );
    sprintf( lsNbr3, "%.0f%%", ldMinPct );
    sprintf( lsNbr4, "%.0f%%", ldMaxPct );
    lsL4P += sprintf( lsL4P, "%9s %9s  ", lsNbr1, lsNbr2 );
 
    lsL5P += sprintf( lsL5P, "(%02d)%s (%02d)%s  ",
                      liMinDays, lsMinDate+5, liMaxDays, lsMaxDate+5 );
    lsL6P += sprintf( lsL6P, "%9s %9s  ", lsNbr3, lsNbr4 );

    // Put section...
    ldMin = 99999.99;
    ldMax = -99999.99;
    liMinDays = 0;
    liMaxDays = 0;
    lsMinDate = (char *)"";
    lsMaxDate = (char *)"";
    for( lcValue = gdPPrices.begin(); lcValue != gdPPrices.end(); lcValue++ )
    {
      lstValue = *lcValue;
      if( lstValue->SellPrice < ldMin )
      {
        ldMin = lstValue->SellPrice;
        ldMinPct = ( ( lstValue->SellPrice - gdPBuyPrice ) / gdPBuyPrice ) * 100;
        liMinDays = DateDiffDaysYYYY_MM_DD( lstValue->TranDate, lstEDate->TranDate );
        lsMinDate = lstValue->TranDate;
      }
      if( lstValue->SellPrice > ldMax )
      {
        ldMax = lstValue->SellPrice;
        ldMaxPct = ( ( lstValue->SellPrice - gdPBuyPrice ) / gdPBuyPrice ) * 100;
        liMaxDays = DateDiffDaysYYYY_MM_DD( lstValue->TranDate, lstEDate->TranDate );
        lsMaxDate = lstValue->TranDate;
      }
    }
    sprintf( lsNbr1, "%.2f", ldMin );
    sprintf( lsNbr2, "%.2f", ldMax );
    sprintf( lsNbr3, "%.0f%%", ldMinPct );
    sprintf( lsNbr4, "%.0f%%", ldMaxPct );
    lsL4P += sprintf( lsL4P, "%9s %9s  ", lsNbr1, lsNbr2 );
 
    lsL5P += sprintf( lsL5P, "(%02d)%s (%02d)%s  ",
                      liMinDays, lsMinDate+5, liMaxDays, lsMaxDate+5 );
    lsL6P += sprintf( lsL6P, "%9s %9s  ", lsNbr3, lsNbr4 );
  }

  printf( "%s\n", lsL1 );
  printf( "%s\n", lsL2 );
  printf( "%s\n", lsL3 );
  printf( "%s\n", lsL4 );
  printf( "%s\n", lsL5 );
  printf( "%s\n\n", lsL6 );
}

//----------------------------------------------------------------
// Function: PrintResults
//----------------------------------------------------------------
void PrintResults2( char *asSymbol, double adStockQuote, std::list<EDATES *> alEDates )
{
  std::list<EDATES *>::iterator  lcEDate;
  std::list<ESVALUES *>::iterator  lcValue;
  EDATES                         *lstEDate;
  ESVALUES                       *lstValue;
  double                         ldCBuyPrice, ldPBuyPrice;
  char                           lsPart1[100];
  char                           lsPart2[100];
  double                         ldCMin, ldCMax;
  double                         ldPMin, ldPMax;
  double                         ldCMinPct, ldCMaxPct;
  double                         ldPMinPct, ldPMaxPct;
  char                           lsNbrCallMin[200];
  char                           lsNbrCallMax[200];
  char                           lsNbrCallMinPct[200];
  char                           lsNbrCallMaxPct[200];
  char                           lsNbrPutMin[200];
  char                           lsNbrPutMax[200];
  char                           lsNbrPutMinPct[200];
  char                           lsNbrPutMaxPct[200];
  char                           lsNbrCallStrikePrice[200];
  char                           lsNbrPutStrikePrice[200];
  char                           lsNbrStockPrice[200];
  char                           lsNbr11[200];
  char                           lsNbrCallBuyPrice[200];
  char                           lsNbrPutBuyPrice[200];
  char                           lsNbrInvested[200];
  char                           lsNbrROI[200];
  char                           lsNbrROIPct[200];
  char                           lsNbrCSell[30];
  char                           lsNbrPSell[30];
  int                            liMinDays, liMaxDays;
  char                           *lsMinDate, *lsMaxDate;
  double                         ldCMinValue, ldCMaxValue;
  double                         ldPMinValue, ldPMaxValue;
  int                            liNbr1, liNbr2, liNbr3, liNbr4;
  double                         ldSellPrice;
  bool                           ldPrintedHeadings = false;
  double                         ldCallStrike, ldPutStrike;
  double                         ldSellLow, ldSellHigh;
  double                         ldCSell, ldPSell;

  for( lcEDate = alEDates.begin(); lcEDate != alEDates.end(); lcEDate++ )
  {
    lstEDate = *lcEDate;

    switch( gcCallHighLow )
    {
      case 'C':
        ldCallStrike = lstEDate->StrikePrice;
        break;
      case 'L':
        ldCallStrike = lstEDate->LowStrike;
        break;
      case 'H':
        ldCallStrike = lstEDate->HighStrike;
        break;
    }

    switch( gcPutHighLow )
    {
      case 'C':
        ldPutStrike = lstEDate->StrikePrice;
        break;
      case 'L':
        ldPutStrike = lstEDate->LowStrike;
        break;
      case 'H':
        ldPutStrike = lstEDate->HighStrike;
        break;
    }

    GetExpireStrikePrices( asSymbol, lstEDate, ldCallStrike, ldPutStrike );

//    printf( "Closed: %.2f   Call: %.2f   Put: %.2f\n",
//            lstEDate->StrikePrice, ldCallStrike, ldPutStrike );

//    if( lstEDate->StrikePrice < gdStrikeLow || lstEDate->StrikePrice > gdStrikeHigh )
//      continue;
    if( ( ldCallStrike < gdStrikeLow || ldCallStrike > gdStrikeHigh ) &&
        ( ldPutStrike < gdStrikeLow || ldPutStrike > gdStrikeHigh ) )
      continue;

    // Call section...
    ldCMin = 99999.99;
    ldCMax = -99999.99;
    ldCBuyPrice = gdCBuyPrice * 100;
    ldSellLow = ldCBuyPrice + ( ldCBuyPrice * gdAutoSellLow );
    ldSellHigh = ldCBuyPrice + ( ldCBuyPrice * gdAutoSellHigh );
    ldCSell = 0;
//    printf( "-- Call Sell low=%.0f   high=%.0f   buyprice=%.0f\n", ldSellLow, ldSellHigh, ldCBuyPrice );

    for( lcValue = gdCPrices.begin(); lcValue != gdCPrices.end(); lcValue++ )
    {
      lstValue = *lcValue;
      ldSellPrice = lstValue->SellPrice * 100;

//      printf( "   CALL: SellPrice: %.0f : %.2f  Date: %s\n", ldSellPrice, lstValue->SellPrice, lstValue->TranDate );

      if( ldSellPrice < ldCMin )
      {
        ldCMin = ldSellPrice;
        ldCMinPct = ( ( ldSellPrice - ldCBuyPrice ) / ldCBuyPrice ) * 100;
      }
      if( ldSellPrice > ldCMax )
      {
        ldCMax = ldSellPrice;
        ldCMaxPct = ( ( ldSellPrice - ldCBuyPrice ) / ldCBuyPrice ) * 100;
      }

      if( ldSellPrice < ldSellLow )
      {
        ldCSell = ldSellLow;
//        ldCSell = ldSellPrice;
//        if( ldCSell < ldSellLow )
//          ldCSell = ldSellLow
        break;
      }

      if( ldSellPrice > ldSellHigh )
      {
//        ldCSell = ldSellHigh;
        ldCSell = ldSellPrice;
//        printf( "### auto sell CALL at HIGH...%.0f\n", ldSellPrice );
        break;
      }
    }

    if( ldCSell == 0 )
      ldCSell = ldSellPrice;
//      ldCSell = ldSellPrice - ldCBuyPrice;

    sprintf( lsNbrCallMin, "%.0f", ldCMin );
    sprintf( lsNbrCallMax, "%.0f", ldCMax );
    sprintf( lsNbrCallMinPct, "%.0f%%", ldCMinPct );
    sprintf( lsNbrCallMaxPct, "%.0f%%", ldCMaxPct );

    // Put section...
    ldPMin = 99999.99;
    ldPMax = -99999.99;
    ldPBuyPrice = gdPBuyPrice * 100;
    ldSellLow = ldPBuyPrice + ( ldPBuyPrice * gdAutoSellLow );
    ldSellHigh = ldPBuyPrice + ( ldPBuyPrice * gdAutoSellHigh );
    ldPSell = 0;
//    printf( "-- Put Sell low=%.0f   high=%.0f  buyprice=%0.f\n", ldSellLow, ldSellHigh, ldPBuyPrice );

    for( lcValue = gdPPrices.begin(); lcValue != gdPPrices.end(); lcValue++ )
    {
      lstValue = *lcValue;
      ldSellPrice = lstValue->SellPrice * 100;

//     printf( "   PUT: SellPrice: %.0f : %.2f  Date: %s\n", ldSellPrice, lstValue->SellPrice, lstValue->TranDate );
 
      if( ldSellPrice < ldPMin )
      {
        ldPMin = ldSellPrice;
        ldPMinPct = ( ( ldSellPrice - ldPBuyPrice ) / ldPBuyPrice ) * 100;
      }
      if( ldSellPrice > ldPMax )
      {
        ldPMax = ldSellPrice;
        ldPMaxPct = ( ( ldSellPrice - ldPBuyPrice ) / ldPBuyPrice ) * 100;
      }

      if( ldSellPrice < ldSellLow )
      {
        ldPSell = ldSellLow;
//        ldPSell = ldSellPrice;
//        ldPSell = ldSellPrice - ldPBuyPrice;
//        printf( "### auto sell PUT at LOW...%.0f\n", ldSellPrice );
        break;
      }

      if( ldSellPrice > ldSellHigh )
      {
//        ldPSell = ldSellHigh;
        ldPSell = ldSellPrice;
//        printf( "### auto sell PUT at HIGH...%.0f\n", ldSellPrice );
        break;
      }
    }

    if( ldPSell == 0 )
      ldPSell = ldSellLow;
//      ldPSell = ldSellPrice - ldPBuyPrice;

    sprintf( lsNbrPutMin, "%.0f", ldPMin );
    sprintf( lsNbrPutMax, "%.0f", ldPMax );
    sprintf( lsNbrPutMinPct, "%.0f%%", ldPMinPct );
    sprintf( lsNbrPutMaxPct, "%.0f%%", ldPMaxPct );

//    sprintf( lsNbrStrikePrice, "%.2f", lstEDate->StrikePrice );
    sprintf( lsNbrCallStrikePrice, "%.2f", ldCallStrike );
    sprintf( lsNbrPutStrikePrice, "%.2f", ldPutStrike );

    sprintf( lsNbrCallBuyPrice, "%.0f", gdCBuyPrice * 100 );
    sprintf( lsNbrPutBuyPrice, "%.0f", gdPBuyPrice * 100 );

    sprintf( lsNbrStockPrice, "%.2f", lstEDate->StockPrice );

    ldCMinValue = ldCBuyPrice + ldPBuyPrice;

//    if( ldCMaxPct > ldPMaxPct )
//    {
//      ldCMaxValue = ldCMax - ldCMinValue;
//    }
//    else
//    {
//      ldCMaxValue = ldPMax - ldCMinValue;
//    }
    ldCMaxValue = ( ldCSell + ldPSell ) - ldCMinValue;

    sprintf( lsNbrInvested, "%.0f", ldCMinValue );
    sprintf( lsNbrROI, "%.0f", ldCMaxValue );
    sprintf( lsNbrROIPct, "%.0f%%", ( ldCMaxValue / ldCMinValue ) * 100 );

    sprintf( lsNbrCSell, "%.0f", ldCSell );
    sprintf( lsNbrPSell, "%.0f", ldPSell );

    if( ldPrintedHeadings == false )
    {
      ldPrintedHeadings = true;
      printf( "Symbol   Quote Tran Date  ExpireDate  "
              " Strike $    Buy $    Low Low Pct   High HighPct SellAmt  "
              "    Strike $     Buy $    Low Low Pct   High HighPct SellAmt  Invest    ROI ROIPct\n" );
      printf( "------ ------- ---------- ---------- "
              "--------- --------- ------ ------- ------ ------- -------  "
              "   --------- --------- ------ ------- ------ ------- -------  ------ ------ ------\n" );
    }

    printf( "%-6s %7s %s %s %9s "
            "C%8s %6s %7s %6s %7s %7s  "
            "|  %9s P%8s %6s %7s %6s %7s  %6s %7s %6s %6s %s %s\n",
            asSymbol,
            lsNbrStockPrice,
            lstEDate->TranDate,
            lstEDate->ExpireDate,
            lsNbrCallStrikePrice,
            lsNbrCallBuyPrice,
            lsNbrCallMin,
            lsNbrCallMinPct,
            lsNbrCallMax,
            lsNbrCallMaxPct,
            lsNbrCSell,
            lsNbrPutStrikePrice,
            lsNbrPutBuyPrice,
            lsNbrPutMin,
            lsNbrPutMinPct,
            lsNbrPutMax,
            lsNbrPutMaxPct,
            lsNbrPSell,
            lsNbrInvested,
            lsNbrROI,
            lsNbrROIPct,
            ldCMaxValue>0 ? "***" : "   ",
            ldCMaxPct>0 && ldPMaxPct>0 ? "+++" : "   " );
  }

  if( ldPrintedHeadings == true )
    printf( "\n" );
}

//----------------------------------------------------------------
// Function: ClearEDateList
//----------------------------------------------------------------
void ClearEDateList( std::list<EDATES *> alEDates )
{
  std::list<EDATES *>::iterator  lcEDate;
  EDATES                         *lstEDate;

  for( lcEDate = alEDates.begin(); lcEDate != alEDates.end(); lcEDate++ )
  {
    lstEDate = *lcEDate;
    free( lstEDate );
  }
}

//----------------------------------------------------------------
// Function: GetExpireStrikePrices
//----------------------------------------------------------------
double GetExpireStrikePrices( char *asSymbol, EDATES *astEDate, double adCallStrike, double adPutStrike )
{
  char         lsSQL[1000];
  MYSQL_RES    *lstResult;
  MYSQL_ROW    lstRow;
  double       ldBuyPrice = 0.0;
  ESVALUES     *lstNewValue;
  double       ldStrike;

  gdCPrices.clear();
  gdPPrices.clear();
  gdCBuyPrice = 0.0;
  gdPBuyPrice = 0.0;

  sprintf( lsSQL,
           "select tran_date, ask_price, bid_price, call_put_ind, strike_price "
           "from   call_put_strikes cps, symbol_daily_calls_puts cp "
           "where  cp.call_put_id = cps.call_put_id "
           "and    symbol='%s' "
           "and    tran_date >= '%s' "
           "and    expire_date='%s' "
           "and    strike_price in (%f,%f,%f) "
           "order by tran_date",
           asSymbol,
           astEDate->TranDate,
           astEDate->ExpireDate,
           astEDate->StrikePrice,
           astEDate->LowStrike,
           astEDate->HighStrike );

  if( mysql_query( gstDBConn, lsSQL ) )
  {
    printf( "GetStrikePrice: SELECT failed: %s\n", mysql_error( gstDBConn ) );
    return 0.0;
  }

  lstResult = mysql_store_result( gstDBConn );

  if( lstResult == NULL )
    return 0.0;

  while( ( lstRow = mysql_fetch_row( lstResult ) ) )
  {
    ldStrike = atof( lstRow[4] );

    if( lstRow[1] )
    {
      if( toupper( lstRow[3][0] ) == 'C' && gdCBuyPrice == 0.0 && ldStrike == adCallStrike )
        gdCBuyPrice = atof( lstRow[1] );
      if( toupper( lstRow[3][0] ) == 'P' && gdPBuyPrice == 0.0 && ldStrike == adPutStrike )
        gdPBuyPrice = atof( lstRow[1] );
    }

    if( lstRow[2] )
    {
      lstNewValue = (ESVALUES *)malloc( sizeof ( ESVALUES ) );
      strcpy( lstNewValue->TranDate, lstRow[0] );
      lstNewValue->SellPrice = atof( lstRow[2] );
      if( toupper( lstRow[3][0] ) == 'C' && ldStrike == adCallStrike )
        gdCPrices.push_back( lstNewValue );
      if( toupper( lstRow[3][0] ) == 'P' && ldStrike == adPutStrike )
        gdPPrices.push_back( lstNewValue );
    }
//    printf( " --  %s : %s : %s : %s  (%.2f)\n", lstRow[0], lstRow[1], lstRow[2], lstRow[3], ldBuyPrice );
  }

  mysql_free_result( lstResult );
  return ldBuyPrice;
}
