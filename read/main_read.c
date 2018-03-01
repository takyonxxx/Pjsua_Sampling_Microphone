#include <stdio.h>
#include <stdlib.h>
#include <string.h>    
#include <sys/types.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>	
#include <unistd.h>    
#include <time.h>    
#include <math.h>   
#include <signal.h>
#include <pj/types.h>
#include <pjlib.h>
#include <pjmedia.h>
#include <pjmedia_audiodev.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#define THIS_FILE   "main.c"
#define SNDCARD_SAMPLING_RATE	96000	
#define MILSEC_KBSP 16000
#define PORT 8888  
#define BUFF_SIZE 321
#define SAMPLE_SIZE 3840
#define FRAME_SIZE 15360
#define CRYPTO_SAMPLE_RATE 16000
#define PTIME 50

//TODO : Make all calculations parametric!
//+- 2147483647 => (( 2^32 )/2) -1  formülü ile çıkarılmaktadır. -1 ile 0 case' ini handle ediyoruz.
static volatile pj_bool_t g_run_app = PJ_TRUE;
char *srvIP = "10.1.10.193";
char log_buffer[250];
static volatile int quit_flag=0;

//Jitter Buffer 
static pjmedia_jbuf *s_buffer = 0;
typedef struct stereo_sample {
    pj_int32_t LEFT;
    pj_int32_t RIGHT;
} stereo_sample_t;

typedef enum { false, true } bool;

struct sockaddr_in si_other;
int s,slen=sizeof(si_other),recv_len;
pj_mutex_t *readMutex;

void die(char *s)
{
    perror(s);
    exit(1);
}

//Opens UDP socket after the media is encrypted.This method is invoked lastly after the media is converted into media signals, and put into a frame.
//The signals located in the buffer is sent to the server( stated with srvip param ).
int udp_start(char* srvip)
{
    if ( (s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
        die("socket");
    }

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_addr.s_addr = INADDR_ANY;
    si_other.sin_port = htons(PORT);

    if (inet_aton(srvip , &si_other.sin_addr) == 0)
    {
        fprintf(stderr, "inet_aton() failed\n");
        return 0;
    }
    else
         printf("Udp has been started Ip: %s Port: %d\n\n",srvip,PORT);

    return s;
}

int to_bitstream(char *src, size_t src_size, void *dst, size_t dst_size)
{
    static unsigned char bitmask[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 }; // x.bit 1 yapmak icin value|= bitmask[x];
                                                                                          // x.bit 0 yapmak icin value&= ~bitmask[x];
    // calculate minimum destination byte count
    int min_dword_size = (src_size % 32 == 0) ? (int)(src_size / 32) : (int)(src_size / 32) + 1;
    if (dst_size/4 < min_dword_size)
        return -1;

    memset(dst, 0, dst_size);

    unsigned char *ptr = (unsigned char *)dst;
    unsigned c;

    int bit  = 0;
    int byte = 0;

    for (c = 0; c < src_size; ++c) {
    //printf("before %02X", ptr[byte]);
        if (src[c] == '0') {
            ptr[byte]&= ~bitmask[bit];
            //printf("- %02X %d.bit 0 yapildi.\n", ptr[byte], bit);
        }
        else if (src[c] == '1') {
            ptr[byte]|=  bitmask[bit];
            //printf("- %02X %d.bit 1 yapildi.\n", ptr[byte], bit);
        }
        else {
            ptr[byte]&= ~bitmask[bit];
            //printf("ERROR: %d.byte 0 veya 1 degildi[%c]! Yine de ilgili bit 0 yapildi:)\n", c, src[c]);
        }

        ++bit;

        if (bit == 8) {
            bit = 0;
            ++byte;
        }
    }
    return min_dword_size;
}

void calculate( int *counter, int *shift, unsigned char *buffer, int *index, bool sign )
{
     int j, division = (*counter) / 6;
     
     if( ((*counter)%6) != 0 )
     {
     	 if( ((*counter)%6) == 3 )
         {                 
         	(*shift)++;                
         	if( (*shift) == 2 )
                {
                	division++;
                        (*shift) = 0;
                }
         }
         else if( ((*counter)%6) > 3 )
         {
         	division++;
         }
     }
     
     for(j = 0; j< division; j++)
     {
         (*index)++;
         buffer[(*index)]= ( sign == true )?'1':'0';
     }
   
    (*counter) = 0; 
}

static void parseFrameValues( stereo_sample_t *samples, pj_size_t samplesPerFrame )
{
   int i, j, value;
   int positiveCounter = 0, negativeCounter = 0;
   static bool isNegativeBitsWritten, isPositiveBitsWritten = false;
   unsigned char udpReadBuf[800], bitStreamBuf[100];
   int bufferIndex = 0, threeBitsNegativeShift = 0, threeBitsPositiveShift = 0;
   
   memset( udpReadBuf, 'Z', 800);
   
   for( i = 0; i < samplesPerFrame/2; i++ )
   {
      value = samples[i].LEFT;
      if(value > 9000000)
      {
          positiveCounter++;
          if( i == samplesPerFrame/2 - 1 )
          {    
              calculate( &positiveCounter, &threeBitsPositiveShift, udpReadBuf, &bufferIndex, true);

              isPositiveBitsWritten = true;
              isNegativeBitsWritten = false;
          }

          if( !isNegativeBitsWritten )
          {
             calculate( &negativeCounter, &threeBitsNegativeShift, udpReadBuf, &bufferIndex, false );
 
             isNegativeBitsWritten = true;
             isPositiveBitsWritten = false;    
          }
      }
      else
      {
          negativeCounter++;
          if( i == samplesPerFrame/2 - 1 )
          {
             calculate( &negativeCounter, &threeBitsNegativeShift, udpReadBuf, &bufferIndex, false);

             isNegativeBitsWritten = true;
             isPositiveBitsWritten = false;
          }
              
          if( !isPositiveBitsWritten )
          { 
              calculate( &positiveCounter, &threeBitsPositiveShift, udpReadBuf, &bufferIndex, true );

              isPositiveBitsWritten = true;
              isNegativeBitsWritten = false;
          }
      }
    }

    int result = to_bitstream( udpReadBuf, sizeof(udpReadBuf), bitStreamBuf, sizeof(bitStreamBuf) );
    if( result > 0 )
    {
       int rp= sendto(s, bitStreamBuf, ( result * 4), 0, (struct sockaddr *)&si_other,slen);
       struct timeval  tv = { 0 };
       gettimeofday(&tv, NULL);
       if(rp<0)
       {
          printf("\n< ERROR writing to SOCKET");
       }
    }
}
 
// Read Jitter Buffer by using another thread and mutex to prevent concurrent access situations.
// You should call this function in 10ms, by using a thread and sleep function. you can not call it via callback functions because it s your implementation. 
static void readFramesInBuffer( stereo_sample_t *samples, pj_size_t samplePerFrame )
{
    pj_status_t status;    
    status = pj_mutex_trylock(readMutex);
    if( status == PJ_SUCCESS )
    {
    	char ch;
    	pjmedia_jbuf_get_frame(s_buffer, samples, &ch );
        status = pj_mutex_unlock(readMutex);
    	if( ch == PJMEDIA_JB_NORMAL_FRAME)
    	{
           parseFrameValues(samples,samplePerFrame);  
    	}
    }
}

static void* thread_proc(pjmedia_aud_param *param)
{
    char buffer[40000];
    pj_pool_t *pool;
    //struct timeval  tv = { 0 };
    stereo_sample_t *samples;
    pool = pj_pool_create_on_buf("thepool", buffer, sizeof(buffer));
    // Use the pool as usual
    samples = pj_pool_alloc(pool, ( param->samples_per_frame *  (param->bits_per_sample / 8)));

    //EXPLANATION : While reading data from buffer, create an array to be able to read the current(oldest)frame.
    //static pj_int32_t samples[(SAMPLE_SIZE)];
    pj_thread_desc desc;
    pj_thread_t *this_thread;
    //unsigned id; //TODO : Use id, pcounter and log the relevant fields.
    pj_status_t rc;

    /*id = *pcounter;
    PJ_UNUSED_ARG(id); 
    PJ_LOG(3,(THIS_FILE, "     thread %d running..", id));
    */
    pj_bzero(desc, sizeof(desc));

    rc = pj_thread_register("thread", desc, &this_thread);
    if (rc != PJ_SUCCESS) {
        PJ_LOG(3,(THIS_FILE, "...error in pj_thread_register"));
        return NULL;
    }

    this_thread = pj_thread_this();
    if (this_thread == NULL) {
        PJ_LOG(3,(THIS_FILE, "...error: pj_thread_this() returns NULL!"));
        return NULL;
    }

    if (pj_thread_get_name(this_thread) == NULL) {
        PJ_LOG(3,(THIS_FILE, "...error: pj_thread_get_name() returns NULL!"));
        return NULL;
    }

    for (;!quit_flag;) {
	readFramesInBuffer(samples, ( param->samples_per_frame) );
	pj_thread_sleep(40);
    }
    //PJ_LOG(3,(THIS_FILE, "     thread %d quitting..", id));
    return NULL;
}

static void sendToBuffer( void *buf, pj_size_t bufSize )
{  
    static pj_int32_t frame_seq = 0;
    pjmedia_jbuf_put_frame(s_buffer, buf, bufSize , frame_seq++);
}

static void wait_thread(pj_thread_t **thread)
{
    while(1)
    {
        pj_thread_sleep(1 * 1000); // wait 10 msec
    }
}

static pj_status_t my_port_get_frame(struct pjmedia_port *this_port, pjmedia_frame *frame)
{
    PJ_LOG(3, (THIS_FILE, "media port: my_port_get_frame %lu", frame->size));

    return PJ_SUCCESS;
}

//If your application makes recording, this callback is triggerred. Otherwise, get callback of null port is triggered.
static pj_status_t my_port_put_frame(struct pjmedia_port *this_port, pjmedia_frame *frame)
{
    pj_status_t rc;
    
    rc = pj_mutex_trylock(readMutex);
    if( rc == PJ_SUCCESS)
    {
    	sendToBuffer(frame->buf, frame->size );
    	rc =  pj_mutex_unlock(readMutex);
    }
    return PJ_SUCCESS;
}

static pj_status_t my_port_on_destroy(struct pjmedia_port *this_port)
{
    PJ_LOG(3, (THIS_FILE, "media port: my_port_on_destroy"));
    return PJ_SUCCESS;
}

static void startSIP(const pjmedia_aud_param *param)
{
    pj_status_t status;
    pjmedia_endpt *med_endpt; //Opaque declar ation of media endpoint. 

    pj_pool_factory *pf; //This structure contains the declaration for pool factory interface.

    pf = pjmedia_aud_subsys_get_pool_factory();//Get the pool factory registered to the audio subsystem.

    status = pjmedia_endpt_create(pf, NULL, 1, &med_endpt); //Create an instance of media endpoint and initialize audio subsystem.
    pj_assert(status == PJ_SUCCESS); 

    pj_pool_t *pool;//This structure describes the memory pool. Only implementors of pool factory need to care about the contents of this structure.

    pool = pj_pool_create(pf, "poolSender", 10*1024*1024, 1*1024*1024, NULL); //Create a new pool from the pool factory. This wrapper will call create_pool member of the pool factory.
    pj_assert(pool != NULL);
    printf("\n 1. Pool Used Size = %d, Pool Capacity = %d \n", pj_pool_get_used_size(pool),pj_pool_get_capacity(pool));

    pj_mutex_create_simple(pool, "readMutex", &readMutex); //Create simple, non-recursive mutex. 
  
    // null port
    pjmedia_port *null_port;//Port interface. 

    status = pjmedia_null_port_create(pool, param->clock_rate, param->channel_count, param->samples_per_frame, param->bits_per_sample, &null_port); //
    pj_assert(status == PJ_SUCCESS); 

    //Jitter buffer creation
    const pj_str_t name = pj_str("JitterBuffer");
    const unsigned PREFETCH = 4;
    
    printf("\n 2. Pool Used Size = %d, Pool Capacity = %d \n", pj_pool_get_used_size(pool),pj_pool_get_capacity(pool));

    status = pjmedia_jbuf_create(pool, &name, (param->samples_per_frame * ((param->bits_per_sample)/8)), PTIME,  100, &s_buffer);

    printf("\n 3. Pool Used Size = %d, Pool Capacity = %d \n", pj_pool_get_used_size(pool),pj_pool_get_capacity(pool));

    pj_assert(status == PJ_SUCCESS);

    status = pjmedia_jbuf_set_fixed(s_buffer, PREFETCH);
    pj_assert(status == PJ_SUCCESS);

    pj_status_t rc;
    pj_thread_t *_thread;

    //Create thread for read operations.
    rc = pj_thread_create(pool, "thread", (pj_thread_proc*)&thread_proc,
                          param,
                          PJ_THREAD_DEFAULT_STACK_SIZE,
                          0, &_thread);

    if (rc != PJ_SUCCESS) {
        return -1010;
    }

    null_port->get_frame  = my_port_get_frame;
    null_port->put_frame  = my_port_put_frame;
    null_port->on_destroy = my_port_on_destroy;

    // sound device port
    pjmedia_snd_port *snd_port; 

    //Create unidirectional sound device port for capturing audio streams from the sound device with the specified parameters
    status = pjmedia_snd_port_create_rec(pool, param->rec_id, param->clock_rate, param->channel_count, param->samples_per_frame, param->bits_per_sample, 0, &snd_port); 
    pj_assert(status == PJ_SUCCESS);

    //Connect a port to the sound device port. If the sound device port has a sound recorder device, then this will start periodic function call to the port's put_frame() function.
    //If the sound device has a sound player device, then this will start periodic function call to the port's get_frame() function
    status = pjmedia_snd_port_connect(snd_port, null_port); 
    pj_assert(status == PJ_SUCCESS);

    pj_thread_sleep(1 * 100); // wait 10 msec

    PJ_LOG(3, (THIS_FILE, "\n\nSending UDP data to server %s  port %d",srvIP,PORT));

    pj_thread_t *thread = 0;
    wait_thread(&thread);

    /*while (g_run_app) {
        pj_thread_sleep(1 * 1000); // wait 1 sec
    }*/
    
    status =  pjmedia_snd_port_destroy(snd_port);//Destroy sound device port.
    pj_assert(status == PJ_SUCCESS);

    status = pjmedia_port_destroy(null_port);
    pj_assert(status == PJ_SUCCESS);

    status = pjmedia_jbuf_destroy(s_buffer);
    pj_assert(status == PJ_SUCCESS);

    pj_pool_release(pool);//Release the pool back to pool factory.

    status = pjmedia_endpt_destroy(med_endpt);
    pj_assert(status == PJ_SUCCESS); // Equal to the statement "if( status == PJ_SUCCESS )"

}

void listAudioDevInfo()
{
    unsigned devCount = pjmedia_aud_dev_count();
    unsigned i;

    for ( i = 0; i < devCount; i++)
    {
        pjmedia_aud_dev_info info;
        pj_status_t status = pjmedia_aud_dev_get_info(i, &info);

        if (status == PJ_SUCCESS)
        {
           sprintf(log_buffer, "Card Num: %2d - Card Name: %s %dHz",i,info.name,info.default_samples_per_sec);
           printf("%s\n",log_buffer);
        }
    }
}

static int main_func(int argc, char *argv[])
{
    pj_status_t status;
    pj_caching_pool cp; //?

    pj_log_set_level(3);// Function definition to set the desired level of verbosity of the logging messages.

    status = pj_init();//Initialize the PJ Library. This function must be called before using the library. 
		       //The purpose of this function is to initialize static library data, such as character table used in random string generation, 
		       //and to initialize operating system dependent functionality (such as WSAStartup() in Windows).
    pj_assert(status == PJ_SUCCESS); //Check during debug build that an expression is true. If the expression computes to false during run-time, 
				     //then the program will stop at the offending statements. For release build, this macro will not do anything.

    PJ_LOG(3, (THIS_FILE, "Process ID: %d", pj_getpid()));

    pj_caching_pool_init(&cp, &pj_pool_factory_default_policy, 4 * 1024 * 1024); //Initialize caching pool.
 
    pjmedia_aud_subsys_init(&cp.factory);//Initialize the audio subsystem. This will register all supported audio device factories to the audio subsystem. 
				         //This function may be called more than once, but each call to this function must have the corresponding pjmedia_aud_subsys_shutdown() call.

    listAudioDevInfo();

    udp_start(srvIP);

    pjmedia_aud_dev_index dev_idx = 10;

    PJ_LOG(3, (THIS_FILE, "Selected audio device id: %d", dev_idx));

    pjmedia_aud_param param;

    status = pjmedia_aud_dev_default_param(dev_idx, &param);
    pj_assert(status == PJ_SUCCESS);

    //PJMEDIA_DIR_ENCODING : Outgoing to network, CAPTURE 
    //PJMEDIA_DIR_DECODING : Incoming from network , PLAYBACK

    param.dir               = PJMEDIA_DIR_CAPTURE; 
    param.clock_rate        = SNDCARD_SAMPLING_RATE;
    param.channel_count     = 2;
    param.samples_per_frame = param.clock_rate * param.channel_count * PTIME / 1000;
    param.bits_per_sample   = 32;   

    printf("\n\nclock_rate : %d\nchannel_count : %d\nsamples_per_frame : %d\nbits_per_sample : %d\n",
           param.clock_rate,param.channel_count,param.samples_per_frame,param.bits_per_sample );

    startSIP(&param);

    pjmedia_aud_subsys_shutdown();//Shutdown the audio subsystem. This will destroy all audio device factories registered in the audio subsystem.
                                                                                       
    pj_caching_pool_destroy(&cp); //Destroy caching pool, and release all the pools in the recycling list.

    PJ_LOG(3, (THIS_FILE, "Process terminated!"));

    pj_shutdown();//Shutdown PJLIB. 
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    return (pj_run_app(&main_func, argc, argv, 0));
}

