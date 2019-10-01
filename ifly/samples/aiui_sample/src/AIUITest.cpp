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
#include <ctime>

#include <sstream>
#include <iostream>
#define __line //printf("%s : %s (%d)\n",__FILE__,__FUNCTION__,__LINE__);
#define IAT_NUM 512

using namespace VA;
string log_file_name="log_file/"+get_time()+".txt";
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

void tts_function(const char* input){
	int ret = MSP_SUCCESS;
	const char* login_params = "appid = 5d836e29, work_dir = .";//登录参数,appid与msc库绑定,请勿随意改动 59a4c2a0
	const char* session_begin_params = "voice_name = xiaoyan, text_encoding = utf8, sample_rate = 16000, speed = 50, volume = 50, pitch = 50, rdn = 2";
	const char* filename = "tts_sample.wav"; //合成的语音文件名称
	const char* text = input; //合成文本
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

string parse_result = "";
static int state = 0;
//事件回调接口，SDK状态，文本，语义结果等都是通过该接口抛出
void TestListener::onEvent(const IAIUIEvent& event) const{
	switch (event.getEventType()) {
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
						parse_result = result_Param["intent"]["answer"]["text"].asString(); // 拿來當判斷的string todo
						cout << parse_result << endl;
						string result = "";
						if(parse_result != "init"){
							cout << parse_result.find("已确认") << endl;
							if(parse_result.find("已确认") > 8 && parse_result.find("已确认") < 20){ // work around
								parse_result = "请问这是什么材料构成的";
							}
							get_user_log(log_file_name, "system", parse_result);
							tts_function(parse_result.c_str());
						}
						state = 1;
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
	string appid = "5d836e29";
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
			string lgiparams = "appid=5d836e29, engine_start=ivw";
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

void AIUITester::wakeup(){
	if (NULL != agent){
		IAIUIMessage * wakeupMsg = IAIUIMessage::create(AIUIConstant::CMD_WAKEUP);
		agent->sendMessage(wakeupMsg);
		wakeupMsg->destroy();
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

void AIUITester::destory(){
	if (NULL != agent){
		agent->destroy();
		agent = NULL;
	}
}

string Int_to_String(int n){
	ostringstream stream;
	stream << n; //n为int类型
	return stream.str();
}

string get_time(){
	// 基于当前系统的当前日期/时间
	time_t now = time(0);
	tm *ltm = localtime(&now);
	string time_= Int_to_String(1900 + ltm->tm_year) +
				  "_" + Int_to_String(1 + ltm->tm_mon) + 
				  "_" + Int_to_String(ltm->tm_mday) + 
				  "_" + Int_to_String(ltm->tm_hour) + 
				  "_" + Int_to_String(ltm->tm_min) + 
				  "_" + Int_to_String(ltm->tm_sec);
	return time_;
}

void get_user_log(string file_name,string user , string data){
	ofstream outfile;
	outfile.open(file_name.c_str(), ios::out|ios::app);
	string log_time = get_time();
	outfile << log_time << "_" << user << "_" << data << endl;
	outfile.close();
}


//接收用户输入命令，调用不同的测试接口
void AIUITester::readCmd(){
	char first[10] = "阿英";
	char sec[10] = "拉";
	int count = 0;
	int count_null = 0;
	
    cout<<"teddy"<<endl;
	while (true){
		createAgent();
    	wakeup();
		char* newline = (char*)calloc(1000, sizeof(char));
		string result = "";
		cout << "while loop count: " << count << endl;
		writeText(first);
		while(state != 1){
			usleep(500);
		}
		if(count == 0){
			printf("Entry = %s\n", first);
			tts_function("你好! 我是阿英, 垃圾分类的问题可以问我");
			get_user_log(log_file_name,"system","你好! 我是阿英, 垃圾分类的问题可以问我");
			destory();
		}else if(count > 0){
			FILE *fd;
			printf("Start Listening...\n");
			system("play -q ding.wav");
			fd = popen("./iat_online_record_sample", "r");
			while((fgets(newline, 256, fd)) != NULL) {
				result = newline;
			}
			if(strstr(newline, "结束") != NULL){
				break;
			}else if ((newline != NULL) && (newline[0] == '\0')){
				printf("count null = %d\n", count_null);
				if(count_null >= 2){
					printf("没有听到我会的, 我先干别的去了, 需要再叫我阿英\n");
					tts_function("没有听到我会的, 我先干别的去了, 需要再叫我阿英");
					get_user_log(log_file_name,"system","没有听到我会的, 我先干别的去了, 需要再叫我阿英");
					break;
				}else{
					state = 0;
					printf("No Speak input!\n");
					writeText(sec);
				}
				count_null++;
			}else{
				state = 0;
				count_null = 0;
				printf("Speak = %s\n", newline);
				get_user_log(log_file_name,"user",newline);
				writeText(result);
			}
			pclose(fd);
		}else{
			printf("Error!\n");
		}
		while(state != 1){
			usleep(50000);
		}
		count++;
		state = 0;
		free(newline);
		destory();
	}
}

void AIUITester::test(){

	AIUISetting::setAIUIDir(TEST_ROOT_DIR);
	AIUISetting::initLogger(LOG_DIR);
	readCmd();
}
