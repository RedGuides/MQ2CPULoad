/********************************************************************************
****
**  	MQ2CpuLoad.cpp : Original code by Dewey2461
****	
*********************************************************************************
****
**		This plugin acts as a CPU load balancer for EQ. 
**
**      Using this plugin you will be able to dynamically adjust which instances 
**		are running on which cores or allow the plugin to automatically make sure
**      instance in the foreground is on its own cpu.  
**
**		/cpu				; shows basic status 
**	    /cpu help			; shows help info
**      /cpu auto			; turns on auto balancing
**      /cpu manual			; turns off auto balancing
**      /cpu set <core>		; manually moves current instance to another core
**      /cpu high			; flags current instance as high priority
**      /cpu low			; flags current instance as low priority
**		/cpu report <level> ; how much debugging info do you want? 
**
****	
*********************************************************************************
****	
**	    What the /cpu status shows
**
**      MQ2CpuLoad::Status for XYZ  
**		Core 1 [ 20 ]   Foobar 21 FPS , Fooclr 20 FPS
**      Core 2 [ 11 ]  >Foomnk 35 FPS
**
**		This shows three characters Foobar, Fooclr, and Foomnk running on a dual 
**		core system with the frame rates shown.
**
**		In front of each character name will be a status symbol: 
**		">" the character has keyboard focus.
**		"*" the character is flaged high priority.
**		"-" the character has not responded in 6s and may be going LD. 
**
****	
*********************************************************************************
****	
**		Released on http://www.Macroquest2.com VIP forums. 
**		Please make any modification available to the author by forum post.
**
**		2012-11-25 by Dewey2461 v 1.0
**			 - Initial coding and testing done on Vista 64 bit with a quad core cpu.
**		2012-11-26 removed dependancy on MQ2FPS. 
**
****
********************************************************************************/

#include "../MQ2Plugin.h"


PreSetup("MQ2CpuLoad");


typedef struct
{
	DWORD	ProcessID;
	HANDLE	ProcessHandle;
	DWORD   AffinityMask;
	DWORD   MoveRequst;
	DWORD	LastUpdate;
	char    CharName[20];
	float	FPS;
	int     Priority;
	int		Foreground;
} trCPUDATA;

#define   MAX_CORES					12				// Max number of Cpu cores to track
#define   MAX_LIST					24				// Max number of EQ instances to track
#define   CLIENT_DISCONNECTED		120000			// How long to wait for a client before calling it dead.
#define   CLIENT_BUSY				3000			// How long to wait for a client to be marked busy.
#define   BALANCE_TIME_FG			6000			// Foreground instance will balance cores every 6s
#define   BALANCE_TIME_BG			12000			// If none are active balance every 12s 

/********************************************************************************
****
**     VARIABLES HERE ARE SHARED BETWEEN ALL INSTANCES -- MAKE SURE ALL ARE INITIALIZED 
****
********************************************************************************/
#pragma comment(linker, "/SECTION:.shr,RWS")
#pragma data_seg(".shr")

trCPUDATA eqList[MAX_LIST] = { {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0} };
int       cpuLoad[MAX_CORES] = { 0,0,0,0,0,0,0,0,0,0,0,0 };
DWORD     cpuLoadUpdated = 0;
DWORD     cpuLoadBalanced = 0;
int		  cpuReporting    = 1;
int		  cpuAutoBalance  = 1;

#pragma data_seg()
/********************************************************************************
****
**     VARIABLE BELOW HERE ARE LOCAL TO EACH INSTANCE 
****
********************************************************************************/

HANDLE		myProcessHandle = 0;
DWORD		myProcessID		= 0;
trCPUDATA  *myCpuData		= NULL;

int	        myCores			= 1;
int			SpewLevel		= 1;

float      *pMQFPS			= NULL;
bool       *pMQForeground	= NULL;

typedef HWND   (__stdcall *fEQW_GetDisplayWindow)(VOID);
fEQW_GetDisplayWindow		EQW_GetDisplayWindow=0;

void GetDisplayWindowAddr()
{
	HMODULE		EQWhMod			= 0;
	if (EQWhMod=GetModuleHandle("eqw.dll"))
	{
		EQW_GetDisplayWindow=(fEQW_GetDisplayWindow)GetProcAddress(EQWhMod,"EQW_GetDisplayWindow");
	}
}

void CpuLoadRemoveDead(void)
{
	int i;
	for (i=0; i<MAX_LIST; i++)
	{
		if (cpuLoadUpdated > eqList[i].LastUpdate + CLIENT_DISCONNECTED)
		{
			memset(&eqList[i],0,sizeof(trCPUDATA));
		}
	}
}


void CpuLoadCalculate(DWORD tick)
{
	int i,n,m;

	if (tick < cpuLoadUpdated + 500) return;
	cpuLoadUpdated = tick;

	memset(cpuLoad,0,sizeof(cpuLoad));

	for (i=0; i<MAX_LIST; i++)
	{
		if (eqList[i].ProcessID)
		{
			m = 1;
			for (n=0; n<myCores; n++)
			{
				if (eqList[i].AffinityMask==m) 
				{
					cpuLoad[n] += 10 + eqList[i].Foreground + eqList[i].Priority;
				}
				m = m * 2;
			}
		}
	}
}


/********************************************************************************
****
**     BALANCER -- Should clean it up but it works and I don't want to break it. 
****
********************************************************************************/

void CpuLoadBalance(void)
{
	int c = 0;		// active eq [C]lients 
	int d = 0;		// sum of v[x] 
	int m = 1;		// mask

	int n = 0;		
	int i = 0;
	int min = 0;
	int max = 0;

	int v[MAX_CORES]; // count of client vs core.
	int z[MAX_LIST];  // map client to core
	int p[MAX_LIST];  // map priority to client/core
	memset(v,0,sizeof(v));
	memset(p,0,sizeof(p));

	for (i=0; i<MAX_LIST; i++)
	{
		p[i] = 100;						// makes code to find lowest priority easier below.
		z[i] = -1;
		if (eqList[i].ProcessID)
		{
			// if someone is busy let the fimish before we try and balance.
			if (cpuLoadUpdated > eqList[i].LastUpdate + CLIENT_BUSY) return;
			p[i] = 10 + eqList[i].Foreground + eqList[i].Priority;
			c++;
			m = 1;
			for (n=0; n<myCores; n++)
			{
				if ((eqList[i].AffinityMask&m)==m) 
				{
					v[n] += p[i];
					z[i]  = n;
					d++;
				}
				m = m * 2;
			}
		}
	}
	
	if (c != d) return;						// someones not done loading - Mask hasen't been set.
			
	min = max = 0;
	for (n=1; n<myCores; n++)
	{
		if (v[min]>v[n]) min = n;
		if (v[max]<v[n]) max = n;
	}

	// If we have a core with 2+ instances and a core move will net a 1+ change.
	if (v[max] >= 20 && v[max]-v[min] > 10) 
	{
		// Find lowest priority instance on max core.
		int l=-1;
		for (i=0; i<MAX_LIST; i++)
		{
			if (z[i] == max)
			{
				if (l==-1) 	l=i;
				if (p[i] < p[l]) l=i;
			}
		}
		// request that instance to move to lowested loaded core.
		if (l!=-1)
		{
			eqList[l].MoveRequst = min+1;
			if (cpuReporting) WriteChatf("MQ2CpuLoad::Balance::Requesting %s move to core %d",eqList[l].CharName,eqList[l].MoveRequst);
		}
	}
}

void CpuLoadShowHelp(int ShowHelp,int ShowStatus);

void CpuLoadUpdate(DWORD tick,int name,int cpu,int load)
{
	if (myCpuData && myProcessID && myProcessHandle)
	{
		//if (cpuReporting) WriteChatf("MQ2CpuLoad::CpuLoadUpdate tick = %d ",myCpuData->LastUpdate);
		if (cpu) 
		{
			DWORD SystemAffinityMask;
			GetProcessAffinityMask(myProcessHandle,&myCpuData->AffinityMask,&SystemAffinityMask);
		}
		if (name)
		{
			PCHARINFO pCharInfo=GetCharInfo();
			if (pCharInfo) 
				strncpy(myCpuData->CharName,pCharInfo->Name,15);
			else
				sprintf(myCpuData->CharName,"%d",myCpuData->ProcessID);
		}
		if (load)
			CpuLoadCalculate(tick);
		myCpuData->LastUpdate = tick;

		if (tick > cpuLoadBalanced+BALANCE_TIME_BG || (myCpuData->Foreground && tick > BALANCE_TIME_FG))
		{
			cpuLoadBalanced = tick;
			CpuLoadRemoveDead();
			CpuLoadCalculate(tick);
			if (cpuAutoBalance)	CpuLoadBalance();
			if (cpuReporting>2) CpuLoadShowHelp(FALSE,TRUE);
		}
	}
}


void CpuLoadINIT(void)
{
	int i;
	SYSTEM_INFO SystemInfo;
	DWORD tick = GetTickCount();
	myProcessHandle = GetCurrentProcess();
	myProcessID		= GetCurrentProcessId();
	GetSystemInfo(&SystemInfo);
	myCores = SystemInfo.dwNumberOfProcessors;
	if (myCores > MAX_CORES) myCores = MAX_CORES;
	for (i=0; i<MAX_LIST; i++)
	{
		if (eqList[i].ProcessID == 0 )
		{
			eqList[i].ProcessID		 = myProcessID;
			eqList[i].ProcessHandle	 = myProcessHandle;
			eqList[i].LastUpdate	 = tick;
			eqList[i].MoveRequst	 = 0;
			eqList[i].CharName[0]    = 0;
			myCpuData = &eqList[i];
			CpuLoadUpdate(tick,TRUE,TRUE,TRUE);
			return;
		}
	}
}




void CpuLoadShowHelp(int ShowHelp,int ShowStatus)
{

	if (ShowHelp) {
		WriteChatf("----------------------------------------------");
		WriteChatf("MQ2CpuLoad Help::");
		WriteChatf("----------------------------------------------");
		WriteChatf("This plugin acts as a CPU load balancer for EQ. ");
		WriteChatf("");
		WriteChatf("Using this plugin you will be able to dynamically adjust which instances ");
		WriteChatf("are running on which cores or allow the plugin to automatically make sure");
		WriteChatf("instance in the foreground is on its own cpu.  ");
		WriteChatf("");
		WriteChatf("/cpu				; shows basic status ");
		WriteChatf("/cpu help			; shows help info");
		WriteChatf("/cpu auto			; turns on auto balancing");
		WriteChatf("/cpu manual			; turns off auto balancing");
		WriteChatf("/cpu set <core>		; manually moves current instance to another core");
		WriteChatf("/cpu high			; flags current instance as high priority");
		WriteChatf("/cpu low			; flags current instance as low priority");
		WriteChatf("/cpu report <level> ; how much debugging info do you want? ");
		WriteChatf("");
	}

	if (ShowHelp || ShowStatus) {
		int i;
		int c;
		int m = 1;
		int n = 0;
		DWORD tick = GetTickCount();
		char szLine[MAX_STRING];
		char szItem[MAX_STRING];

		CpuLoadUpdate(tick,TRUE,TRUE,TRUE);
		WriteChatf("MQ2CpuLoad::Status %s",ShowHelp?"":"use /cpu help for details");
		if (cpuReporting>0 && !ShowHelp)	{
			WriteChatf("MQ2CpuLoad::Priority for %s is %s ",myCpuData->CharName,myCpuData->Priority?"high":"low");
			WriteChatf("MQ2CpuLoad::AutoBalance is %s ",cpuAutoBalance?"auto":"manual");
			WriteChatf("MQ2CpuLoad::Reporting is level %d ",cpuReporting);
		}

		for (c=0; c<myCores; c++)
		{
			n = 0;
			szLine[0] = ' ';
			szLine[1] = 0;
			szItem[0] = 0;
			char p;
			for (i=0; i<MAX_LIST; i++)
			{
				if (eqList[i].ProcessID && eqList[i].AffinityMask==m) 
				{
					p = eqList[i].Priority?'*':' ';
					p = eqList[i].Foreground?'>':p;
					if (tick > eqList[i].LastUpdate + CLIENT_BUSY) p = '-';
					
					sprintf(szItem,"%s %c%s %2.0f fps  ",
						(n>0?" , ":""),p,eqList[i].CharName,eqList[i].FPS);
					strcat(szLine,szItem); 
					n++;
				}
			}
			WriteChatf("  cpu%d - [%2d] %s ",c+1,cpuLoad[c],szLine);
			m = m * 2;
		}
	}

	if (ShowHelp)
	{
		WriteChatf("In front of each character name will be a status symbol: [ > , * , - ] ");
		WriteChatf("  >  the character has keyboard focus.");
		WriteChatf("  *  the character is flaged high priority.");
		WriteChatf("  -  the character has not responded in 6s and may be going LD. ");
	}
}

void CpuLoadSet(int c)
{
	int m = 1;
	int n = 0;
	if (!myCpuData) return;
	
	for (n=0; n<myCores; n++)
	{
		if (n+1==c) 
		{
			if (cpuReporting>0)	WriteChatf("MQ2CpuLoad::Moving %s to core %d",myCpuData->CharName,c);
			SetProcessAffinityMask(myProcessHandle,m);
			return;
		}
		m = m * 2;
	}
}



void LoadINIFile(void)
{
	char szTemp[256];
	char  *pName = GetCharInfo()->Name;
	if (!myCpuData || !pName) return;
	
	GetPrivateProfileString("Priority", pName		,"0",szTemp	,256,INIFileName);	myCpuData->Priority = atoi(szTemp);
	GetPrivateProfileString("Settings","Reporting"	,"0",szTemp	,256,INIFileName);	cpuReporting		= atoi(szTemp);
	GetPrivateProfileString("Settings","AutoBalance","1",szTemp	,256,INIFileName);	cpuAutoBalance		= atoi(szTemp);
}

void SaveINIFile(void)
{
	char  *pName = GetCharInfo()->Name;
	char  szTemp1[256];
	
	if (!myCpuData && !pName) return;

	sprintf(szTemp1,"%d",myCpuData->Priority);	WritePrivateProfileString("Priority",pName			, szTemp1, INIFileName);
	sprintf(szTemp1,"%d",cpuReporting);			WritePrivateProfileString("Settings","Reporting"	, szTemp1, INIFileName);	
	sprintf(szTemp1,"%d",cpuAutoBalance);		WritePrivateProfileString("Settings","cpuAutoBalance", szTemp1, INIFileName);	

	WriteChatf("----------------------------------");
	WriteChatf("MQ2CpuLoad::Priority for %s is %s ",myCpuData->CharName,myCpuData->Priority?"high":"low");
	WriteChatf("MQ2CpuLoad::AutoBalance is %s ",cpuAutoBalance?"auto":"manual");
	WriteChatf("MQ2CpuLoad::Reporting is level %d ",cpuReporting);
}


void CpuLoadCommand(PSPAWNINFO pCHAR, PCHAR szLine) 
{
	int  ShowHelp = 0;
	int  ShowStat = 0;
	char Arg1[MAX_STRING]; GetArg(Arg1,szLine,1);
	char Arg2[MAX_STRING]; GetArg(Arg2,szLine,2);
	char Arg3[MAX_STRING]; GetArg(Arg3,szLine,3);

	if (!myCpuData)	{
		WriteChatf("MQ2CpuLoad - Can't find data - aborting command (should not happen!)");
		return;
	}
	if (stricmp(Arg1,"")==0)			  ShowStat = 1;
	if (stricmp(Arg1,"help")==0)		  ShowHelp = 1;
	if (stricmp(Arg1,"set")==0)		 	  CpuLoadSet(atoi(Arg2));
	if (strnicmp(Arg1,"hi"		,2)==0)	{ myCpuData->Priority = 1;		SaveINIFile(); }
	if (strnicmp(Arg1,"lo"		,2)==0)	{ myCpuData->Priority = 0;		SaveINIFile(); }
	if (strnicmp(Arg1,"report"	,3)==0)	{ cpuReporting = atoi(Arg2);	SaveINIFile(); }
	if (strnicmp(Arg1,"auto"	,4)==0) { cpuAutoBalance = 1;			SaveINIFile(); }
	if (strnicmp(Arg1,"manual"	,3)==0) { cpuAutoBalance = 0;			SaveINIFile(); }

	
	CpuLoadShowHelp(ShowHelp ,ShowStat);

}




// Called once, when the plugin is to initialize
PLUGIN_API VOID InitializePlugin(VOID)
{
    DebugSpewAlways("Initializing MQ2CpuLoad");
	AddCommand("/cpu",CpuLoadCommand);
	GetDisplayWindowAddr();
	CpuLoadINIT();


    //Add commands, MQ2Data items, hooks, etc.
    //AddCommand("/mycommand",MyCommand);
    //AddXMLFile("MQUI_MyXMLFile.xml");
    //bmMyBenchmark=AddMQ2Benchmark("My Benchmark Name");
}

// Called once, when the plugin is to shutdown
PLUGIN_API VOID ShutdownPlugin(VOID)
{
    DebugSpewAlways("Shutting down MQ2CpuLoad");
	RemoveCommand("/cpu");
	if (myCpuData) {
		memset(myCpuData,0,sizeof(trCPUDATA));
		myCpuData = NULL;
	}
    //Remove commands, MQ2Data items, hooks, etc.
    //RemoveMQ2Benchmark(bmMyBenchmark);
    //RemoveCommand("/mycommand");
    //RemoveXMLFile("MQUI_MyXMLFile.xml");
}

// Called after entering a new zone
PLUGIN_API VOID OnZoned(VOID)
{
    DebugSpewAlways("MQ2CpuLoad::OnZoned()");
	CpuLoadUpdate(GetTickCount(),TRUE,TRUE,TRUE);
}


// Called once directly after initialization, and then every time the gamestate changes
PLUGIN_API VOID SetGameState(DWORD GameState)
{
    DebugSpewAlways("MQ2CpuLoad::SetGameState()");
    if (GameState==GAMESTATE_INGAME)
	{
		LoadINIFile();
		CpuLoadUpdate(GetTickCount(),TRUE,TRUE,TRUE);
	}
    // create custom windows if theyre not set up, etc
}


// This is called every time MQ pulses
PLUGIN_API VOID OnPulse(VOID)
{
	static DWORD tick  = 0;
	static DWORD frame = 0;
	static DWORD tock  = 0;

	tick = GetTickCount();
	if (!tock) tock = tick;
	if (tick - tock > 500)
	{
		if (myCpuData)
			myCpuData->FPS = 1000.0 * frame / ( 1.0 * tick - tock);
		tock = tick;
		frame = 0;
	}
	frame++;

	if (!myCpuData) return;
	myCpuData->Foreground = 0;

	// Code for finding if the window has focus taken from MQ2FPS 
    HWND EQhWnd=*(HWND*)EQADDR_HWND;
    if (EQW_GetDisplayWindow)		EQhWnd=EQW_GetDisplayWindow();
	myCpuData->Foreground = (GetForegroundWindow()==EQhWnd );

	if (tick > myCpuData->LastUpdate + 1000) {
		CpuLoadUpdate(tick,FALSE,FALSE,FALSE);
		if (myCpuData->MoveRequst) {
			CpuLoadSet(myCpuData->MoveRequst);
			myCpuData->MoveRequst = 0;
			CpuLoadUpdate(tick,TRUE,TRUE,TRUE);
			if (cpuReporting>1) CpuLoadShowHelp(FALSE,TRUE);
		}
	}

}

