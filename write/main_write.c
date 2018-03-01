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
#include <unistd.h>

#define THIS_FILE   "main.c"
#define SNDCARD_SAMPLING_RATE  96000	
#define MILSEC_KBSP  16000
#define PORT 8888  
#define BUFF_SIZE 321 //TODO : Make parametric
#define SAMPLE_SIZE 3840 //TODO : Make parametric

static const int SAMPLE_MAX =  2000000000 ;//2154000000;   // highest value for S32 format
static const int SAMPLE_MIN =  0 ;   // lowest  value for S32 format, may be set 0

typedef struct stereo_sample {
    pj_int32_t LEFT;
    pj_int32_t RIGHT;
} stereo_sample_t;

static volatile pj_bool_t g_run_app = PJ_TRUE;
typedef enum { false, true } bool;

char *srvIP ="127.0.0.1";
char log_buffer[250];
int PTIME = 50;
int CRYPTO_SAMPLE_RATE = 16000;
int packetLength; 

//Jitter Buffer 
static pjmedia_jbuf *s_buffer = 0;
static pj_lock_t *s_lock = 0;

struct sockaddr_in srvAddr;
struct sockaddr_in cliAddr;
int fd;
long recvlen;
socklen_t clientlen;

int s,slen,recv_len;
pj_mutex_t *writeMutex;

void die(char *s)
{
    perror(s);
    exit(1);
}

//Opens UDP socket after the media is encrypted.This method is invoked lastly after the media is converted into media signals, and put into a frame.
//The signals located in the buffer is sent to the server( stated with srvip param ).
void udp_start(char* srvip)
{
    if((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
        printf("\n cannot create socket");
        return 0;
    }

    srvAddr.sin_family = AF_INET;
    srvAddr.sin_port = htons(8888);
    srvAddr.sin_addr.s_addr = inet_addr("10.1.10.197");
    memset(srvAddr.sin_zero, '\0', sizeof srvAddr.sin_zero);

    if(bind(fd, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) < 0){
        printf("\n cannot bind");
        return 0;
    }
 
    clientlen = sizeof(cliAddr);
}

int from_bitstream(char *src, size_t src_size, void *dst, size_t dst_size)
{
    static unsigned char bitmask[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 }; // x.bit 1 yapmak icin value|= bitmask[x];

    memset(dst, 0, dst_size);

    char *ptr = (char *)dst;
    unsigned c;

    int bit  = 0;
    int byte = 0;

    for (c = 0; c < src_size * 8; ++c) {
        if( src[byte] & bitmask[bit] )
        {
            ptr[c] = '1';
        }
        else
        {
            ptr[c] = '0';
        }

        ++bit;

        if (bit == 8) {
            bit = 0;
            ++byte;
        }
    }
    return 0;
}

static void sendToBuffer( void *buf, pj_size_t bufSize )
{
    static pj_int32_t frame_seq = 0;
    pjmedia_jbuf_put_frame(s_buffer, buf, bufSize , frame_seq++);
}

void convert_bits( stereo_sample_t *samples, long unsigned sample_count)
{
    int i, j, counter = 0;
    static char buf2[100], buf[800];
    recvlen = recvfrom(fd, buf2, 100, 0, (struct sockaddr *)&cliAddr, &clientlen);

    if (recvlen < 0) {
        printf("\n cannot recvfrom()");
        return 0;
    }

    from_bitstream(buf2, 100, buf, 800);

    for (i = 0; i < 800 ; ++i)
    {
        if( buf[i] == '0' )
        {
            for(j = counter; j < counter+6; j++)
            {
                samples[j].LEFT = SAMPLE_MIN;
                samples[j].RIGHT = SAMPLE_MIN;
            }
        }
        else if(  buf[i] == '1')
        {
            for(j = counter; j < counter+6; j++)
            {
                samples[j].LEFT = SAMPLE_MAX;
                samples[j].RIGHT = SAMPLE_MAX;
            }
        }
        counter += 6;
    }
    //Put bits into jitter buffer after converting them into 6 bits formats
     sendToBuffer((void*)samples, 38400 );
}

static void* thread_proc(pjmedia_aud_param *param)
{
    char buffer[40000];
    pj_pool_t *pool;
    stereo_sample_t *samples;
    pool = pj_pool_create_on_buf("thepool", buffer, sizeof(buffer));
    // Use the pool as usual
    samples = pj_pool_alloc(pool, ( param->samples_per_frame * (param->bits_per_sample / 8)));

    //EXPLANATION : While reading data from buffer, create an array to be able to read the current(oldest)frame.
    //static pj_int32_t samples[(SAMPLE_SIZE)];
    pj_thread_desc desc;
    pj_thread_t *this_thread;
    unsigned id;
    pj_status_t rc;

    pj_bzero(desc, sizeof(desc));

    rc = pj_thread_register("thread", desc, &this_thread);
    if (rc != PJ_SUCCESS) {
        PJ_LOG(3,(THIS_FILE, "...error in pj_thread_register"));
        return NULL;
    }

    /* Test that pj_thread_this() works */
    this_thread = pj_thread_this();
    if (this_thread == NULL) {
        PJ_LOG(3,(THIS_FILE, "...error: pj_thread_this() returns NULL!"));
        return NULL;
    }

    /* Test that pj_thread_get_name() works */
    if (pj_thread_get_name(this_thread) == NULL) {
        PJ_LOG(3,(THIS_FILE, "...error: pj_thread_get_name() returns NULL!"));
        return NULL;
    }

    /* Main loop */
    while(1)
    {
        convert_bits(samples, param->samples_per_frame );
    }

    PJ_LOG(3,(THIS_FILE, "     thread %d quitting..", id));
    return NULL;
}

static void wait_thread(pj_thread_t **thread)
{
    while(1)
    {
        pj_thread_sleep(1 * 1000); // wait 10 msec
    }
}

static pj_status_t my_port_get_frame(struct pjmedia_port *port, pjmedia_frame *frame)
{
     char ftype;
     pjmedia_jbuf_get_frame(s_buffer, frame->buf, &ftype);
     if (ftype == PJMEDIA_JB_NORMAL_FRAME)
     {}
     
     frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
     return PJ_SUCCESS;
}

//Uygulaman Recording yapıyorsa, bu callback function tetiklenir sürekli olarak. Aksi takdirde yayım yaparsa, null port' un get ile ilgili callback function' ı tetiklenir.
static pj_status_t my_port_put_frame(struct pjmedia_port *this_port, pjmedia_frame *frame)
{
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
    pj_assert(status == PJ_SUCCESS); // ASK : Is pj_assert the same with if statement in C++ ?

    pj_pool_t *pool;//This structure describes the memory pool. Only implementors of pool factory need to care about the contents of this structure.

    pool = pj_pool_create(pf, "pool1", 10*1024*1024, 1*1024*1024, NULL); //Create a new pool from the pool factory. This wrapper will call create_pool member of the pool factory.
    pj_assert(pool != NULL);

    status = pj_mutex_create_simple(pool, "writeMutex", &writeMutex); //Create simple, non-recursive mutex. 
    pj_assert(status == PJ_SUCCESS);
 
    // null port
    pjmedia_port *null_port;//Port interface. 

    status = pjmedia_null_port_create(pool, param->clock_rate, param->channel_count, param->samples_per_frame, param->bits_per_sample, &null_port); //
    pj_assert(status == PJ_SUCCESS); 

    //Jitter buffer creation
    const pj_str_t name = pj_str("JitterBuffer");
    const unsigned PREFETCH = 4;

    status= pjmedia_jbuf_create(pool, &name, (param->samples_per_frame * ((param->bits_per_sample)/8)), PTIME, 100 , &s_buffer);
    pj_assert(status == PJ_SUCCESS);
    
    status = pjmedia_jbuf_set_fixed(s_buffer, PREFETCH);
    pj_assert(status == PJ_SUCCESS);
    
    status = pj_lock_create_simple_mutex(pool, "SyncBufLockObj", &s_lock);
    pj_assert(status == PJ_SUCCESS);
 
    //Create thread for read operations.
    pj_status_t rc;
    pj_thread_t *_thread;

    rc = pj_thread_create(pool, "writeThread", (pj_thread_proc*)&thread_proc,
                          param,
                          PJ_THREAD_DEFAULT_STACK_SIZE,
                          0, &_thread);

    if (rc != PJ_SUCCESS) {
        return -1010;
    }
    
    pj_thread_sleep(800);
  
    null_port->get_frame  = my_port_get_frame;
    null_port->put_frame  = my_port_put_frame;
    null_port->on_destroy = my_port_on_destroy;

    // sound device port
    pjmedia_snd_port *snd_port; 

    //Create unidirectional sound device port for capturing audio streams from the sound device with the specified parameters
    status = pjmedia_snd_port_create_player(pool, param->play_id, param->clock_rate, param->channel_count, param->samples_per_frame, param->bits_per_sample, 0, &snd_port); 
    pj_assert(status == PJ_SUCCESS);

    //Connect a port to the sound device port. If the sound device port has a sound recorder device, then this will start periodic function call to the port's put_frame() function.
    //If the sound device has a sound player device, then this will start periodic function call to the port's get_frame() function
    status = pjmedia_snd_port_connect(snd_port, null_port); 
    pj_assert(status == PJ_SUCCESS);

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
      
    status = pj_lock_destroy(s_lock);
    pj_assert(status == PJ_SUCCESS);

}

void listAudioDevInfo()
{
    printf("\nSound Card List:\n\n");
    unsigned devCount = pjmedia_aud_dev_count();
    unsigned i;

    for ( i = 0; i < devCount; i++)
    {
        pjmedia_aud_dev_info info;
        pj_status_t status = pjmedia_aud_dev_get_info(i, &info);

        if (status == PJ_SUCCESS)
        {
           sprintf(log_buffer, "Card Num: %2d - Card Name: %s %dHz , Driver = %s ",i,info.name,info.default_samples_per_sec,info.driver);
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

    const char *drv_name = "ALSA";
    const char *dev_name = "hw:CARD=OnurAudioCodec1,DEV=0";

    pjmedia_aud_dev_index dev_idx;

    status = pjmedia_aud_dev_lookup(drv_name, dev_name, &dev_idx);
    pj_assert(status == PJ_SUCCESS);

    PJ_LOG(3, (THIS_FILE, "Selected Audio device id: %d", dev_idx));

    udp_start(srvIP);

    //create_udp_listener();
    
    PJ_LOG(3, (THIS_FILE, "Selected audio device id: %d", dev_idx));

    pjmedia_aud_param param;

    status = pjmedia_aud_dev_default_param(dev_idx, &param);
    pj_assert(status == PJ_SUCCESS);

    //PJMEDIA_DIR_ENCODING : Outgoing to network, CAPTURE 
    //PJMEDIA_DIR_DECODING : Incoming from network , PLAYBACK

    param.dir               = PJMEDIA_DIR_PLAYBACK; 
    param.clock_rate        = SNDCARD_SAMPLING_RATE;
    param.channel_count     = 2;
    param.samples_per_frame = param.clock_rate * param.channel_count * PTIME / 1000;
    param.bits_per_sample   = 32;   
    printf("\n play_id = %d, rec_id = %d \n", param.rec_id, param.play_id);
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

