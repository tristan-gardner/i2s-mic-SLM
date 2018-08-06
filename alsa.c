
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <limits.h>
#include <math.h>

// this program only depends on alsa-lib:
// ===========================================
// $ sudo apt-get -y install libasound2-dev
// $ gcc -lasound alsa.c
// $ ./a.out


#define RATE (48000)
#define NUM_IN_CHANNELS (2)
#define SOME_REASONABLE_NUMBER_OF_FRAMES (2000)


float calculateAverage(float average, float new_sample, float old_sample,  int sample_count){
        average -= old_sample/(float)sample_count;
        average += new_sample/(float)sample_count;
        return average;
}

//helper funtion for debugging
void printArray(float arr[], int arr_size, int marker){
    for(int i = 0; i < arr_size; i+= marker){
        fprintf(stderr,"%f, ", arr[i]);
    }
    fprintf(stderr,"\n ");
}

int main(int argc, char **argv)
{
    snd_pcm_t *capture_handle = NULL;

    if (argc > 1)
        if (snd_pcm_open(&capture_handle, argv[1], SND_PCM_STREAM_CAPTURE, 0) < 0)
            capture_handle = NULL;

    if (capture_handle == NULL)
    {
        fprintf(stderr, "unable to open output device\n\n");
        fprintf(stderr, "usage: %s device\n", argv[0]);
        fprintf(stderr, "available devices:\n");


        int card = -1;
        snd_ctl_card_info_t *info;
        snd_ctl_card_info_alloca(&info);
        for (; snd_card_next(&card) >= 0 && card >= 0;)
        {
            snd_ctl_t *handle;
            char name[32];
            sprintf(name, "hw:%d", card);
            if (snd_ctl_open(&handle, name, 0) >= 0)
            {
                if (snd_ctl_card_info(handle, info) >= 0)
                    fprintf(stderr, "device '%s': %s\n", name, snd_ctl_card_info_get_name(info));

                snd_ctl_close(handle);
            }
        }
        return -1;
    }

    snd_pcm_hw_params_t *hw_params = NULL;
    if (snd_pcm_hw_params_malloc(&hw_params) < 0)
    {
        fprintf(stderr, "snd_pcm_hw_params_malloc failed\n");
        return -1;
    }

    if (snd_pcm_hw_params_any(capture_handle, hw_params) < 0)
    {
        fprintf(stderr, "snd_pcm_hw_params_any failed\n");
        return -1;
    }

    if (snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
    {
        fprintf(stderr, "snd_pcm_hw_params_set_access failed\n");
        return -1;
    }

    if (snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S32_LE) < 0)
    {
        fprintf(stderr, "snd_pcm_hw_params_set_format failed\n");
        return -1;
    }

    if (snd_pcm_hw_params_set_rate(capture_handle, hw_params, RATE, 0) < 0)
    {
        fprintf(stderr, "snd_pcm_hw_params_set_rate failed\n");
        return -1;
    }

    if (snd_pcm_hw_params_set_channels(capture_handle, hw_params, NUM_IN_CHANNELS) < 0)
    {
        fprintf(stderr, "snd_pcm_hw_params_set_channels failed\n");
        return -1;
    }

    if (snd_pcm_hw_params(capture_handle, hw_params) < 0)
    {
        fprintf(stderr, "snd_pcm_hw_params failed\n");
        return -1;
    }

    if (snd_pcm_prepare(capture_handle) < 0)
    {
        fprintf(stderr, "snd_pcm_prepare failed\n");
        return -1;
    }

    snd_pcm_hw_params_free(hw_params);

    int bufferSizeInFrames = SOME_REASONABLE_NUMBER_OF_FRAMES;
    int bufferSizeInSamples = bufferSizeInFrames * NUM_IN_CHANNELS;
    int bufferSizeInBytes = bufferSizeInSamples * sizeof(int32_t);

    char *in = malloc(bufferSizeInBytes);

        int* samples = (int*) in;

    //change array_size to alter how many powers are averaged together
    int array_size = 100;//possible bug, average does not seem to calculate correctly if array_size is large and divider is small (i.e 240,2)
    float average_array[array_size];
    int average_array_index = 0;
    float old_average_item;
    float average = 0;

    //Check array variables
    //check_array's size is relative to average_array's
    int divider = 4;
    int check_array_size = array_size/divider;
    float check_array[check_array_size];
    float check_array_average = 0;
    int check_array_index = 0;
    float old_check_item;
    float check_average;

    float currentMaxValue = -400;
    float currentMinValue = 400;

    //dB_buffer is difference between average and new measurements needed to trigger an alert, difference of 6 means the sound has doubled in loudness
    //smaller value meabs it is more sensitive to changes
    float dB_buffer = 4;

    //bool workaround
    int true = 1;
    int false = 0;
    int average_array_full = false;
    int check_array_full = false;

    if (!in)
    {
        fprintf(stderr, "no mem\n");
        return -1;
    }



    fprintf(stderr,"Listening\n");
    while (1)
    {
        unsigned long numframesthistime = snd_pcm_readi(capture_handle, in, SOME_REASONABLE_NUMBER_OF_FRAMES);
    if (numframesthistime < 1)
        {
            fprintf(stderr, "Error snd_pcm_writei\n");
            return -1;
        }

        size_t bytes_to_write = NUM_IN_CHANNELS * numframesthistime * sizeof(int32_t);

        char *from = in;
        while (bytes_to_write)
        {
            ssize_t written_this_time = write(1, from, bytes_to_write);
            if (written_this_time < 1)
            {
                fprintf(stderr, "write failed\n");
                return -1;
            }
            from += written_this_time;
            bytes_to_write -= written_this_time;
        char buffer[32];
        }

    //loop over the buffer and calculate a power level for that segment
    float power = 0;
    for (int i=0; i<numframesthistime; ++i)
        {
                int curSampleL = samples[2*i];

        // divide cursample by the max positive number in 32 bit signed values
        float curSample = curSampleL / (float)(0x7fffffff);
        power += curSample * curSample;
        }
    power /= numframesthistime;
    power = 20*log10f(sqrtf(power));

    //calculate a local average to check against a larger but still recent average, when items expire from this average they are calculated into the larger one
    check_array_index %= check_array_size;
    old_check_item = check_array[check_array_index];
    check_array[check_array_index] = power;
    check_array_index += 1;
    if(check_array_full == true){
        check_average = calculateAverage(check_average, power, old_check_item, check_array_size);
    }
    else{
        check_average += power;
    }
//  fprintf(stderr, "%f -> check[%ld] ", power, check_array_index);

    if(check_array_index == check_array_size){
                if(check_array_full == false){
//          fprintf(stderr,"\n %f / %ld = %f \n",check_average, check_array_size, check_average / check_array_size);
                        check_average /= check_array_size;
                }
                check_array_full = true;
        fprintf(stderr,"check average = %f \n",check_average);
//      printArray(check_array,check_array_size,1);
        }

    //keep track of a certain amount of recent power levels to see if new samples are particularly loud/quiet
    if(check_array_full == true){
        average_array_index %= array_size;
        old_average_item = average_array[average_array_index];
        average_array[average_array_index] = old_check_item;
        average_array_index += 1;

        if(average_array_full == true){
            average = calculateAverage(average, old_check_item, old_average_item, array_size);
//          fprintf(stderr, "%f - %f + %f \n", average, old_average_item/array_size, old_check_item/array_size);
        }
        else{
            average += old_check_item;
        }
//      fprintf(stderr, "-> %f -> average[%ld]\n", old_check_item, average_array_index);

        if(average_array_index == array_size){
            if(average_array_full == false){
//              fprintf(stderr,"\n %f / %ld = %f\n",average, array_size, average/array_size);
                average /= array_size;
            }
            average_array_full = true;
            fprintf(stderr,"average = %f \n",average);
//          printArray(average_array,array_size,1);
        }

    }
    //If setting a baseline soundlevel for a scene, it may be helpful to know the expected minimum and maximum power levels

    if(power > currentMaxValue){
        currentMaxValue = power;
    }
    if(power < currentMinValue){
                currentMinValue = power;
        }


    //Todo send information to flock/speaker to tell them to play softer/louder and/or another added sound to make up for change in ambient noise instead of the print statements

    if(average_array_full == true && check_array_index == check_array_size){
        //increase in sound level compared to average of last array_size readings
        if(check_average-average > dB_buffer){
            fprintf(stderr, "Sound level increase (%f -  %f > %f)  \n",check_average,average,dB_buffer);
        }
        //decrease
        if(check_average-average < 0-dB_buffer){
            fprintf(stderr, "Sound level decrease (%f -  %f < -%f)  \n",check_average,average,dB_buffer);
        }
    }
}
    free(in);

    if (snd_pcm_close(capture_handle) < 0)
    {
        fprintf(stderr, "snd_pcm_close failed\n");
    }
    return 0;
}

