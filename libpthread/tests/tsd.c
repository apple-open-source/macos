#include <assert.h>
#include <pthread.h>
#include <stdio.h>

void *ptr = NULL;

void destructor(void *value)
{
	ptr = value;
}

void *thread(void *param)
{
	int res;

	pthread_key_t key = *(pthread_key_t *)param;
	res = pthread_setspecific(key, (void *)0x12345678);
	assert(res == 0);
	void *value = pthread_getspecific(key);

	pthread_key_t key2;
	res = pthread_key_create(&key, NULL);
	assert(res == 0);
	res = pthread_setspecific(key, (void *)0x55555555);
	assert(res == 0);

	return value;
}

int main(int argc, char *argv[])
{
	int res;
	pthread_key_t key;

	res = pthread_key_create(&key, destructor);
	assert(res == 0);
	printf("key = %ld\n", key);

	pthread_t p = NULL;
	res = pthread_create(&p, NULL, thread, &key);
	assert(res == 0);

	void *value = NULL;
	res = pthread_join(p, &value);
	printf("value = %p\n", value);
	printf("ptr = %p\n", ptr);

	assert(ptr == value);

	res = pthread_key_delete(key);
	assert(res == 0);

	return 0;
}

