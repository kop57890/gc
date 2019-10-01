﻿#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "../../include/msp_cmn.h"
#include "../../include/qivw.h"
#include "../../include/msp_errors.h"

#include "asr_record.h"
#include "formats.h"
#include "linuxrec.h"
#include "speech_recognizer.h"

#include <iostream>
#include "../aiui_sample/src/AIUITest.h"

using namespace std;
using namespace aiui;

#define IVW_AUDIO_FILE_NAME "audio/awake.pcm"
#define FRAME_LEN	640 //16k采样率的16bit音频，一帧的大小为640B, 时长20ms

#define DEFAULT_FORMAT		\
{\
	WAVE_FORMAT_PCM,	\
	1,			\
	16000,			\
	32000,			\
	2,			\
	16,			\
	sizeof(WAVEFORMATEX)	\
}

int16_t g_order = ORDER_NONE;
BOOL g_is_order_publiced = FALSE;
UserData asr_data;

static int record_state = MSP_AUDIO_SAMPLE_CONTINUE;
struct recorder *recorder;
BOOL g_is_awaken_succeed = FALSE;



void sleep_ms(int ms)
{
	usleep(ms * 1000);
}

/* the record call back */
static void iat_cb(char *data, unsigned long len, void *user_para)
{
	int errcode;
	const char *session_id = (const char *)user_para;

	if(len == 0 || data == NULL)
		return;
	if(!g_is_awaken_succeed){
		errcode = QIVWAudioWrite(session_id, (const void *)data, len, record_state);
	}
	if (MSP_SUCCESS != errcode)
	{
		printf("QIVWAudioWrite failed! error code:%d\n",errcode);
		int ret = stop_record(recorder);
		if (ret != 0) {
			printf("Stop failed! \n");
			//return -E_SR_RECORDFAIL;
		}
		wait_for_rec_stop(recorder, (unsigned int)-1);
		QIVWAudioWrite(session_id, NULL, 0, MSP_AUDIO_SAMPLE_LAST);
		record_state = MSP_AUDIO_SAMPLE_LAST;
		g_is_awaken_succeed = FALSE;
	}
	if(record_state == MSP_AUDIO_SAMPLE_FIRST){
		record_state = MSP_AUDIO_SAMPLE_CONTINUE;
	}
}

int cb_ivw_msg_proc( const char *sessionID, int msg, int param1, int param2, const void *info, void *userData )
{

  if (MSP_IVW_MSG_ERROR == msg) //唤醒出错消息
  {
    printf("\n\nMSP_IVW_MSG_ERROR errCode = %d\n\n", param1);
    g_is_awaken_succeed = FALSE;
    record_state = MSP_AUDIO_SAMPLE_LAST;
  }else if (MSP_IVW_MSG_WAKEUP == msg) //唤醒成功消息
  {
    printf("\n\nMSP_IVW_MSG_WAKEUP result = %s\n\n", (char*)info);
    g_is_awaken_succeed = TRUE;
    record_state = MSP_AUDIO_SAMPLE_LAST;
  }
  int ret = stop_record(recorder);
	if (ret != 0) {
		printf("Stop failed! \n");
	}else{
		printf("stop success\n");
	}
	
	//wait_for_rec_stop(recorder, (unsigned int)-1);
	//QIVWAudioWrite(sessionID, NULL, 0, MSP_AUDIO_SAMPLE_LAST);
  
  return 0;
}


void run_ivw(const char *grammar_list,  const char* session_begin_params)
{
	const char *session_id = NULL;
	int err_code = MSP_SUCCESS;
	char sse_hints[128];

	WAVEFORMATEX wavfmt = DEFAULT_FORMAT;
	wavfmt.nSamplesPerSec = SAMPLE_RATE_16K;
	wavfmt.nAvgBytesPerSec = wavfmt.nBlockAlign * wavfmt.nSamplesPerSec;

//start QIVW
	session_id=QIVWSessionBegin(grammar_list, session_begin_params, &err_code);
	if (err_code != MSP_SUCCESS)
	{
		printf("QIVWSessionBegin failed! error code:%d\n",err_code);
		goto exit;
	}

	err_code = QIVWRegisterNotify(session_id, cb_ivw_msg_proc,NULL);
	if (err_code != MSP_SUCCESS)
	{
		snprintf(sse_hints, sizeof(sse_hints), "QIVWRegisterNotify errorCode=%d", err_code);
		printf("QIVWRegisterNotify failed! error code:%d\n",err_code);
		goto exit;
	}

//start record
	err_code = create_recorder(&recorder, iat_cb, (void*)session_id);
	if (recorder == NULL || err_code != 0) {
			printf("create recorder failed: %d\n", err_code);
			err_code = -E_SR_RECORDFAIL;
			goto exit;
	}

	err_code = open_recorder(recorder, get_default_input_dev(), &wavfmt);
	if (err_code != 0) {
		printf("recorder open failed: %d\n", err_code);
		err_code = -E_SR_RECORDFAIL;
		goto exit;
	}

	err_code = start_record(recorder);
	if (err_code != 0) {
		printf("start record failed: %d\n", err_code);
		err_code = -E_SR_RECORDFAIL;
		goto exit;
	}

	record_state = MSP_AUDIO_SAMPLE_FIRST;

	while(record_state != MSP_AUDIO_SAMPLE_LAST)
	{
		sleep_ms(200); //模拟人说话时间间隙，10帧的音频时长为200ms
		printf("waiting for awaken%d\n", record_state);
	}

	snprintf(sse_hints, sizeof(sse_hints), "success");

exit:
	if (NULL != session_id)
	{
		QIVWSessionEnd(session_id, sse_hints);	
	}
	
}


int main(int argc, char* argv[])
{
	int         ret       = MSP_SUCCESS;
	//const char *lgi_param = "appid = 5d3fde6d, work_dir = .";
	const char *lgi_param = "appid = 5d836e29, work_dir = .";
	const char *ssb_param = "ivw_threshold=0:1450,sst=wakeup,ivw_res_path =fo|res/ivw/wakeupresource.jet";

	ret = MSPLogin(NULL, NULL, lgi_param);
	if (MSP_SUCCESS != ret)
	{
		printf("MSPLogin failed, error code: %d.\n", ret);
		goto exit ;//登录失败，退出登录
	}
	
	//run_ivw(NULL, IVW_AUDIO_FILE_NAME, ssb_param); 
	while(1)
	{
		run_ivw(NULL, ssb_param); 

		if(g_is_awaken_succeed==TRUE)
		{
			//system("cd ~/source/gc/ifly_good/samples/aiui_sample/build/");
			//system("./demo");
			AIUITester t;
	        t.test();
		}
	}
	//sleep_ms(2000);
exit:
	printf("按任意键退出 ...\n");
	getchar();
	MSPLogout(); //退出登录
	return 0;
}