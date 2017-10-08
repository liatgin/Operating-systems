//Includes
#include <sys/time.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include "osm.h"

//size of max name.
#define HOSTSIZE    256

// global inner struct 
timeMeasurmentStructure s;


/** 
 * An empty function for function-time measurment.
 **/
static void foo()
{

}
/*
 * We choose not to implement this function
 * Still corresponding with the api..
 */
int osm_init()
{
    return 0;
}


/*
 * We choose not to implement this function
 * Still corresponding with the api..
 */
int osm_finalizer()
{
    return 0;
}

/* Time measurement function for any of the 4 requested operations.
*  input : 
*  "type"- an integer, representing the operation we would like to measure:
*  1 - simple operation, 2 - function, 3- syscall, 4 disk operation
*  other - not allowed.
* 
*  iterations - an integer, number of iterations
*
*  returns time in nano-seconds upon success,
*  and -1 upon failure.
*/

static double all_osm_time(int type, unsigned int iterations)
{

    if (iterations == 0)
    {
        iterations = 1000;
    }
    int i = 0, res1 = 0, res2 = 0;

    struct timeval before;
    struct timeval after;

    //operation
    if (type == 1)
    {
        //res1 is intentionally duplicated and not outside the if's,
        //so it won't effect the measure times.
        res1 = gettimeofday(&before,NULL);
        int a = 1, b = 0;
        for (i = 1; i <= iterations; i++)
        {
            a & b;
        }
    }
        //function
    else if (type == 2)
    {
        res1 = gettimeofday(&before,NULL);
        for (i = 1; i <= iterations; i++)
        {
            foo();
        }
    }
        //syscall
    else if (type == 3)
    {
        res1 = gettimeofday(&before,NULL);
        for (i = 1; i <= iterations; i++)
        {
            OSM_NULLSYSCALL;
        }
    }
        //disk
    else if (type == 4)
    {
        FILE * fp;

        res1 = gettimeofday(&before,NULL);
        //making 2 operations. ceil so will have integers.
        iterations = ceil(iterations/2);
        for (i = 1; i <= iterations; i++)
        {
            fp = fopen("file.txt","w+" );
            if (fp == NULL)
            {
                return -1;
            }
            if (fclose(fp) == EOF)
            {
                return -1;
            }
        }

        iterations =  iterations * 2; // original number of iterations



    }
    else
    {
        return -1;
    }


    if (res1 == -1 || res2 == -1)
    {
        return -1;
    }
    res2 = gettimeofday(&after,NULL);
    double time_diff_usec = (double) (after.tv_usec - before.tv_usec);
    double time_diff_sec = (double) (after.tv_sec - before.tv_sec);
    double time = ((time_diff_sec * 1000000) + time_diff_usec) * 1000;
    time = time/ iterations;
    return time;
}

/* Time measurement function for a simple arithmetic operation.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_operation_time(unsigned int iterations)
{
    double time = all_osm_time(1, iterations);
    return time;
}


/* Time measurement function for an empty function call.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_function_time(unsigned int iterations)
{
    double time = all_osm_time(2, iterations);
    return time;
}


/* Time measurement function for an empty trap into the operating system.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */

double osm_syscall_time(unsigned int iterations)
{
    double time = all_osm_time(3, iterations);
    return time;

}



/* Time measurement function for accessing the disk.
   returns time in nano-seconds upon success,
   and -1 upon failure.
   */
double osm_disk_time(unsigned int iterations)
{
    double time = all_osm_time(4, iterations);
    return time;
}


timeMeasurmentStructure measureTimes (unsigned int operation_iterations,
                                      unsigned int function_iterations,
                                      unsigned int syscall_iterations,
                                      unsigned int disk_iterations)
{
    s.machineName = (char*) malloc(sizeof(char) * HOSTSIZE);
    if(s.machineName == NULL)
    {
        perror("The memory allocation failed");
        exit(1);
    }
    gethostname(s.machineName,HOSTSIZE);
    s.instructionTimeNanoSecond = osm_operation_time(operation_iterations);
    s.functionTimeNanoSecond = osm_function_time(function_iterations);
    s.trapTimeNanoSecond = osm_syscall_time(syscall_iterations);
    s.diskTimeNanoSecond = osm_disk_time(disk_iterations);
    s.functionInstructionRatio = s.functionTimeNanoSecond / \
     s.instructionTimeNanoSecond;
    s.trapInstructionRatio = s.trapTimeNanoSecond / s.instructionTimeNanoSecond;
    s.diskInstructionRatio = s.diskTimeNanoSecond / s.instructionTimeNanoSecond;
    return s;
}




