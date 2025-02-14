#ifndef AVS2SUP_H
#define AVS2SUP_H
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*ProgressCallback)(int progress);

// 注册回调函数
void avs2sup_reg_callback(ProgressCallback callback);

void avs2sup_stop();

int avs2sup_process(const char* avsFile, const char* outFileName, const char* langCode, const char* video_format, const char* frame_rate);

#ifdef __cplusplus
}
#endif
#endif // AVS2SUP_H
