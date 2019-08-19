#include "AIUITest.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fstream>
#include "jsoncpp/json/json.h"
#include <stdio.h>
#include <unistd.h>
#include "qtts.h"
#include "msp_cmn.h"
#include "../../../include/msp_errors.h"
#define __line //printf("%s : %s (%d)\n",__FILE__,__FUNCTION__,__LINE__);

using namespace VA;
typedef struct _wave_pcm_hdr{

	char            riff[4];                // = "RIFF"
	int		size_8;                 // = FileSize - 8
	char            wave[4];                // = "WAVE"
	char            fmt[4];                 // = "fmt "
	int		fmt_size;		// = 下一个结构体的大小 : 16
	short int       format_tag;             // = PCM : 1
	short int       channels;               // = 通道数 : 1
	int		samples_per_sec;        // = 采样率 : 8000 | 6000 | 11025 | 16000
	int		avg_bytes_per_sec;      // = 每秒字节数 : samples_per_sec * bits_per_sample / 8
	short int       block_align;            // = 每采样点字节数 : wBitsPerSample / 8
	short int       bits_per_sample;        // = 量化比特数: 8 | 16
	char            data[4];                // = "data";
	int		data_size;              // = 纯数据长度 : FileSize - 44 
} wave_pcm_hdr;

/* 默认wav音频头部数据 */

wave_pcm_hdr default_wav_hdr = {

	{ 'R', 'I', 'F', 'F' },
	0,
	{'W', 'A', 'V', 'E'},
	{'f', 'm', 't', ' '},
	16,
	1,
	1,
	16000,
	32000,
	2,
	16,
	{'d', 'a', 't', 'a'},
	0  
};
/* 文本合成 */
int text_to_speech(const char* src_text, const char* des_path, const char* params){

	int ret = -1;
	FILE* fp = NULL;
	const char* sessionID = NULL;
	unsigned int audio_len = 0;
	wave_pcm_hdr wav_hdr = default_wav_hdr;
	int synth_status = MSP_TTS_FLAG_STILL_HAVE_DATA;
	if (NULL == src_text || NULL == des_path)	{
		printf("params is error!\n");
		return ret;
	}
	fp = fopen(des_path, "wb");
	if (NULL == fp)	{
		printf("open %s error.\n", des_path);
		return ret;
	}
	/* 开始合成 */
	sessionID = QTTSSessionBegin(params, &ret);
	if (MSP_SUCCESS != ret)	{
		printf("QTTSSessionBegin failed, error code: %d.\n", ret);
		fclose(fp);
		return ret;
	}

	ret = QTTSTextPut(sessionID, src_text, (unsigned int)strlen(src_text), NULL);
	if (MSP_SUCCESS != ret)	{
		printf("QTTSTextPut failed, error code: %d.\n",ret);
		QTTSSessionEnd(sessionID, "TextPutError");
		fclose(fp);
		return ret;
	}
	fwrite(&wav_hdr, sizeof(wav_hdr) ,1, fp); //添加wav音频头，使用采样率为16000
	while (1) {
		/* 获取合成音频 */
		const void* data = QTTSAudioGet(sessionID, &audio_len, &synth_status, &ret);
		if (MSP_SUCCESS != ret)
			break;
		if (NULL != data){
			fwrite(data, audio_len, 1, fp);
		    wav_hdr.data_size += audio_len; //计算data_size大小
		}
		if (MSP_TTS_FLAG_DATA_END == synth_status)
			break;
		usleep(150*1000); //防止频繁占用CPU
	}
	if (MSP_SUCCESS != ret)	{
		printf("QTTSAudioGet failed, error code: %d.\n",ret);
		QTTSSessionEnd(sessionID, "AudioGetError");
		fclose(fp);
		return ret;
	}

	/* 修正wav文件头数据的大小 */
	wav_hdr.size_8 += wav_hdr.data_size + (sizeof(wav_hdr) - 8);
	/* 将修正过的数据写回文件头部,音频文件为wav格式 */
	fseek(fp, 4, 0);
	fwrite(&wav_hdr.size_8,sizeof(wav_hdr.size_8), 1, fp); //写入size_8的值
	fseek(fp, 40, 0); //将文件指针偏移到存储data_size值的位置
	fwrite(&wav_hdr.data_size,sizeof(wav_hdr.data_size), 1, fp); //写入data_size的值
	fclose(fp);
	fp = NULL;
	/* 合成完毕 */
	ret = QTTSSessionEnd(sessionID, "Normal");
	if (MSP_SUCCESS != ret)	{
		printf("QTTSSessionEnd failed, error code: %d.\n",ret);
	}
	return ret;
}

static const string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
string parse_result = "";
//循环写入测试音频，每次写1278B
bool WriteAudioThread::threadLoop()
{
	char audio[1280];
	int len = mFileHelper->read(audio, 1280);
	if (len > 0){
		Buffer* buffer = Buffer::alloc(len);//申请的内存会在sdk内部释放
		memcpy(buffer->data(), audio, len);

		IAIUIMessage * writeMsg = IAIUIMessage::create(AIUIConstant::CMD_WRITE,
			0, 0,  "data_type=audio,sample_rate=16000", buffer);	

		if (NULL != mAgent)
		{
			mAgent->sendMessage(writeMsg);
		}		
		writeMsg->destroy();
		usleep(40 * 1000);
	} else {
		if (mRepeat){
			mFileHelper->rewindReadFile();
		} else {
			IAIUIMessage * stopWrite = IAIUIMessage::create(AIUIConstant::CMD_STOP_WRITE,
				0, 0, "data_type=audio,sample_rate=16000");
			if (NULL != mAgent){
				mAgent->sendMessage(stopWrite);
			}
			stopWrite->destroy();
			mFileHelper->closeReadFile();
			mRun = false;
		}
	}
	return mRun;
}

void* WriteAudioThread::thread_proc(void * paramptr){
	WriteAudioThread * self = (WriteAudioThread *)paramptr;

	while (1) {
		if (! self->threadLoop())
			break;
	}
	return NULL;
}

WriteAudioThread::WriteAudioThread(IAIUIAgent* agent, const string& audioPath, bool repeat):
mAgent(agent), mAudioPath(audioPath), mRepeat(repeat), mRun(true), mFileHelper(NULL)
,thread_created(false){
	mFileHelper = new FileUtil::DataFileHelper("");
	mFileHelper->openReadFile(mAudioPath, false);
}

WriteAudioThread::~WriteAudioThread(){
	if (NULL != mFileHelper){
		delete mFileHelper;
		mFileHelper = NULL;
	}
}

void WriteAudioThread::stopRun(){
    if (thread_created) {
        mRun = false;
        void * retarg;
        pthread_join(thread_id, &retarg);
        thread_created = false;
    }
}

bool WriteAudioThread::run(){
    if (thread_created == false) {
        int rc = pthread_create(&thread_id, NULL, thread_proc, this);
        if (rc != 0) {
            exit(-1);
        }
        thread_created = true;
        return true;
    }
    return false;
}

// string mSyncSid;

//事件回调接口，SDK状态，文本，语义结果等都是通过该接口抛出
void TestListener::onEvent(const IAIUIEvent& event) const{
	switch (event.getEventType()) {
	//SDK 状态回调
	case AIUIConstant::EVENT_STATE:
		{
			switch (event.getArg1()) {
			case AIUIConstant::STATE_IDLE:
				{
					// cout << "EVENT_STATE:" << "IDLE" << endl;
				} break;

			case AIUIConstant::STATE_READY:
				{
					// cout << "EVENT_STATE:" << "READY" << endl;
				} break;

			case AIUIConstant::STATE_WORKING:
				{
					// cout << "EVENT_STATE:" << "WORKING" << endl;
				} break;
			}
		} break;

	//唤醒事件回调
	case AIUIConstant::EVENT_WAKEUP:
		{
			// cout << "EVENT_WAKEUP:" << event.getInfo() << endl;
		} break;

	//休眠事件回调
	case AIUIConstant::EVENT_SLEEP:
		{
			cout << "EVENT_SLEEP:arg1=" << event.getArg1() << endl;
		} break;

	//VAD事件回调，如找到前后端点
	case AIUIConstant::EVENT_VAD:
		{
			switch (event.getArg1()) {
			case AIUIConstant::VAD_BOS:
				{
					cout << "EVENT_VAD:" << "BOS" << endl;
				} break;

			case AIUIConstant::VAD_EOS:
				{
					cout << "EVENT_VAD:" << "EOS" << endl;
				} break;

			case AIUIConstant::VAD_VOL:
				{
					//	cout << "EVENT_VAD:" << "VOL" << endl;
				} break;
			}
		} break;

	//最重要的结果事件回调
	case AIUIConstant::EVENT_RESULT:
		{
			Json::Value bizParamJson;
			Json::Reader reader;
			
			if (!reader.parse(event.getInfo(), bizParamJson, false)) {
				cout << "parse error!" << endl << event.getInfo() << endl;
				break;
			}
			Json::Value data = (bizParamJson["data"])[0];
			Json::Value params = data["params"];
			Json::Value content = (data["content"])[0];
			string sub = params["sub"].asString();
			if (sub == "nlp"){
				Json::Value empty;
				Json::Value contentId = content.get("cnt_id", empty);

				if (contentId.empty()){
					cout << "Content Id is empty" << endl;
					break;
				}

				string cnt_id = contentId.asString();
				int dataLen = 0;
				const char* buffer = event.getData()->getBinary(cnt_id.c_str(), &dataLen);
				string resultStr;
				Json::Value result_Param;
				Json::Reader result_reader;
						
				if (NULL != buffer){
					resultStr = string(buffer, dataLen);					
					if (!reader.parse(resultStr.c_str(), result_Param, false)) {
						cout << "parse error!" << endl << resultStr << endl;
						break;
					}
					if(result_Param["intent"]["answer"]["text"] != "null"){
						parse_result = result_Param["intent"]["answer"]["text"].asString();
						cout << parse_result << endl;
						string result = "";
						int ret = MSP_SUCCESS;
						// 59a4c2a0
						const char* login_params = "appid = 5d3fde6d, work_dir = .";//登录参数,appid与msc库绑定,请勿随意改动
						const char* session_begin_params = "voice_name = xiaoyan, text_encoding = utf8, sample_rate = 16000, speed = 50, volume = 50, pitch = 50, rdn = 2";
						const char* filename = "tts_sample.wav"; //合成的语音文件名称
						const char* text = parse_result.c_str();; //合成文本
						ret = MSPLogin(NULL, NULL, login_params);//第一个参数是用户名，第二个参数是密码，第三个参数是登录参数，用户名和密码可在http://www.xfyun.cn注册获取
						if (MSP_SUCCESS != ret)	{
							printf("MSPLogin failed, error code: %d.\n", ret);
						}
						ret = text_to_speech(text, filename, session_begin_params);
						if (MSP_SUCCESS != ret){
							printf("text_to_speech failed, error code: %d.\n", ret);
						}
						MSPLogout();
						system("play -q tts_sample.wav");
					}
				} else {
					cout << "buffer is NULL" << endl;
				}
			}
		}
		break;
	case AIUIConstant::EVENT_ERROR:
		{
			cout << "EVENT_ERROR:" << event.getArg1() << endl;
			cout << " ERROR info is " << event.getInfo() << endl;
		} break;
	}
}

AIUITester::AIUITester() : agent(NULL), writeThread(NULL)
{

}

AIUITester::~AIUITester(){
	if (agent != NULL){
		agent->destroy();
		agent = NULL;
	}
}

TestListener::TestListener(){
	mTtsFileHelper = new FileUtil::DataFileHelper("");
}

TestListener::~TestListener(){
	if (mTtsFileHelper != NULL){
		delete mTtsFileHelper;
		mTtsFileHelper = NULL;
	}
}

//创建AIUI代理
void AIUITester::createAgent(){
	string appid = "5d3fde6d";
	Json::Value paramJson;
	Json::Value appidJson;

	appidJson["appid"] = appid;
	
	string fileParam = FileUtil::readFileAsString(CFG_FILE_PATH);
	Json::Reader reader;
	if(reader.parse(fileParam, paramJson, false))
	{
		paramJson["login"] = appidJson;
		//for ivw support
		string wakeup_mode = paramJson["speech"]["wakeup_mode"].asString();
		//如果在aiui.cfg中设置了唤醒模式为ivw唤醒，那么需要对设置的唤醒资源路径作处理，并且设置唤醒的libmsc.so的路径为当前路径
		if(wakeup_mode == "ivw"){
			//readme中有说明，使用libmsc.so唤醒库，需要调用MSPLogin()先登录
			string lgiparams = "appid=5d3fde6d,engine_start=ivw";
			printf("sssssssssss\n");
			int ret = MSP_SUCCESS;
			ret = MSPLogin(NULL, NULL, lgiparams.c_str());
			printf("xxxxxxxxx %d\n", ret);
			if (MSP_SUCCESS != ret){
				printf("MSPLogin failed, error code: %d.\n", ret);
				exit(0);
			}
			string ivw_res_path = paramJson["ivw"]["res_path"].asString();
			if(!ivw_res_path.empty()){
				ivw_res_path = "fo|" + ivw_res_path;
				paramJson["ivw"]["res_path"] = ivw_res_path;
			}
			string ivw_lib_path = "/home/vic/awake/libs/x64/libmsc.so";
			paramJson["ivw"]["msc_lib_path"] = ivw_lib_path;
			// MSPLogout();
		}
		//end

		Json::FastWriter writer;
		string paramStr = writer.write(paramJson);
		agent = IAIUIAgent::createAgent(paramStr.c_str(), &listener);
	}else{
		cout << "aiui.cfg has something wrong!" << endl;
	}
}

/*
	外部唤醒接口，通过发送CMD_WAKEUP命令对SDK进行外部唤醒，发送该命令后，SDK会进入working状态，用户就可以与SDK进行交互。
*/
void AIUITester::wakeup(){
	if (NULL != agent){
		IAIUIMessage * wakeupMsg = IAIUIMessage::create(AIUIConstant::CMD_WAKEUP);
		agent->sendMessage(wakeupMsg);
		wakeupMsg->destroy();
	}
}

//写入测试音频
void AIUITester::write(bool repeat){
	if (agent == NULL){
		cout << "write anget == null" << endl;
		return;
	}

	if (writeThread == NULL) {
		writeThread = new WriteAudioThread(agent, TEST_AUDIO_PATH,  repeat);
		writeThread->run();
	}	
}

void AIUITester::stopWriteThread(){
	if (writeThread) {
		writeThread->stopRun();
		delete writeThread;
		writeThread = NULL;
	}
}

void AIUITester::reset(){
	if (NULL != agent){
		IAIUIMessage * resetMsg = IAIUIMessage::create(AIUIConstant::CMD_RESET);
		agent->sendMessage(resetMsg);
		resetMsg->destroy();
	}
}

//文本语义测试接口
void AIUITester::writeText(string stt){
	if (NULL != agent){
		string text = stt;
		// textData内存会在Message在内部处理完后自动release掉
		Buffer* textData = Buffer::alloc(text.length());
		text.copy((char*) textData->data(), text.length());

		IAIUIMessage * writeMsg = IAIUIMessage::create(AIUIConstant::CMD_WRITE,
			0,0, "data_type=text", textData);	
		agent->sendMessage(writeMsg);
		writeMsg->destroy();
	}
}

string AIUITester::encode(const unsigned char* bytes_to_encode, unsigned int in_len) {
	std::string ret;
	int i = 0;
	int j = 0;
	unsigned char char_array_3[3];
	unsigned char char_array_4[4];

	while (in_len--) {
		char_array_3[i++] = *(bytes_to_encode++);
		if (i == 3) {
			char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
			char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
			char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
			char_array_4[3] = char_array_3[2] & 0x3f;

			for(i = 0; (i <4) ; i++){
				ret += base64_chars[char_array_4[i]];
			}
			i = 0;
		}
	}

	if(i){
		for(j = i; j < 3; j++){
			char_array_3[j] = '\0';
		}
		char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
		char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
		char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
		char_array_4[3] = char_array_3[2] & 0x3f;

		for (j = 0; (j < i + 1); j++){
			ret += base64_chars[char_array_4[j]];
		}
		while((i++ < 3)){
			ret += '=';
		}
	}
	return ret;
}

void AIUITester::destory(){
	stopWriteThread();
	if (NULL != agent){
		agent->destroy();
		agent = NULL;
	}
}
//接收用户输入命令，调用不同的测试接口
void AIUITester::readCmd(){
	string result = "";
	char first[10] = "阿英";
	int count = 0;
	createAgent();
    wakeup();
	while (true){
		// printf("count = %d\n", count);
		char newline[256];
		if(count == 0){
			writeText(first);
		}else if(count > 0){
			FILE *fd;
			printf("Start Listening...\n");
			fd = popen("./iat_online_record_sample", "r");
			while((fgets(newline, 256, fd)) != NULL) {
				result = newline;
			}
			if(strstr(newline, "结束") != NULL){
				// printf("xxxx = %s\n", newline);
				exit(0);
			}else if (strstr(newline, "垃圾") != NULL){
				// printf("xxxx = %s\n", newline);
				writeText(result);
			}else{
				writeText(first);
			}
			pclose(fd);
		}else{
			printf("Error!\n");
		}
		count++;
		cin.ignore();
	}
}

void AIUITester::test(){

	AIUISetting::setAIUIDir(TEST_ROOT_DIR);
	AIUISetting::initLogger(LOG_DIR);
	readCmd();
}
