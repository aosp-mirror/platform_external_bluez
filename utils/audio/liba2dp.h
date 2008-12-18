#ifdef __cplusplus
extern "C" {
#endif

typedef void* a2dpData;

int a2dp_init(const char* address, int rate, int channels, a2dpData* dataPtr);
int a2dp_write(a2dpData data, const void* buffer, int count);
void a2dp_cleanup(a2dpData data);

#ifdef __cplusplus
}
#endif
