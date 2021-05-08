#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

    /*      Konsol için renkler      */
#define WARNING "\x1B[31m"  //RED
#define ANNOUNCE "\x1B[32m" //GREEN
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define PTND "\x1B[35m"  //MAG
#define TITLE "\x1B[36m" //CYN
#define RESET "\x1B[0m"

/*  DEFINE  */
#define ROOM_SIZE 3 // her test odasındaki koltuk sayısı
#define UNIT_SIZE 8 // hastahane içerisinde bulunan test odası

#define TEST_TIME 15 // testin yapımı için geçecek olan süre
#define WAIT_TIME 5
#define PATIENT_SIZE 45 // testin uygulanacağı hasta sayısı

int rndValue(int parameter);
int rndUnitID();
void *patient(void *n);
void *test(void *n);

/*  semaphores    */
sem_t hospital;                 //
sem_t seats_of_unit[UNIT_SIZE]; // her test odası için koltuklar
sem_t is_unit_full[UNIT_SIZE];  // odada o an için test yapılıp yapılmadığını tutuyor
sem_t order_mtx;                // test odasının sırasını tutmak için
sem_t patient_mtx;              // hastaların odanın dolmasını bekleme için

int testedPatient = 0;
int inUnit = -1;
int unit = -1;

int main()
{
    int i = 0;
    int testUnitID[UNIT_SIZE];
    int patientID[PATIENT_SIZE];

    pthread_t patients[PATIENT_SIZE];
    pthread_t testUnit[UNIT_SIZE];

    for (i = 0; i < PATIENT_SIZE; i++)
    {
        patientID[i] = i;
    }

    for (i = 0; i < UNIT_SIZE; i++)
    {
        testUnitID[i] = i;
    }

    sem_init(&hospital, 0, UNIT_SIZE);
    sem_init(&order_mtx, 0, 1);
    sem_init(&patient_mtx, 0, 1);

    for (i = 0; i < UNIT_SIZE; i++)
    {
        sem_init(&seats_of_unit[i], 0, ROOM_SIZE); // her test odasında 3 er koltuk bulunacak
    }

    for (i = 0; i < UNIT_SIZE; i++)
    {
        sem_init(&is_unit_full[i], 0, 1); // odanın full & busy state kontrolü için
    }

    for (i = 0; i < UNIT_SIZE; i++)
    {
        pthread_create(&testUnit[i], NULL, test, (void *)&testUnitID[i]);
    }

    for (i = 0; i < PATIENT_SIZE; i++)
    {
        pthread_create(&patients[i], NULL, patient, (void *)&patientID[i]);
    }

    for (i = 0; i < PATIENT_SIZE; i++)
    {
        pthread_join(patients[i], NULL);
    }

    for (i = 0; i < UNIT_SIZE; i++)
    {
        pthread_join(testUnit[i], NULL);
    }

    printf("\n\t\t--- %d patients tested in hospital---", testedPatient);
    return 0;
}

int rndValue(int parameter)
{
    //srand(time(parameter));
    return (rand() % parameter);
}

int rndUnitID()
{
    inUnit++;
    if (inUnit % ROOM_SIZE == 0)
    {
        unit++;
    }

    return unit % UNIT_SIZE;
}

void *patient(void *n)
{
    sem_wait(&patient_mtx);
    sleep(rndValue(WAIT_TIME));
    int id = *(int *)n;
    sem_wait(&hospital);
    printf("->%sPatient P%d in hospital%s\n", PTND, id, RESET);

    int randomUnitID = -1;
    randomUnitID = rndUnitID();
    sem_wait(&seats_of_unit[randomUnitID]);
    printf("->%sPatient P%d went to %sUnit-%d\n", PTND, id, RESET, (randomUnitID + 1));
    sem_post(&hospital);
}

void *test(void *n)
{
    int i = 0;
    int value;
    while (1)
    {
        sem_post(&patient_mtx);
        sleep(2);
        if (testedPatient > PATIENT_SIZE) //Test yapılan hasta sayısına erişilince threadi sonlandır
            break;

        sem_wait(&order_mtx);

        printf("\t######\t\t%sTest Unit Status%s\t######\n", TITLE, RESET);
        for (i = 0; i < UNIT_SIZE; i++)
        {
            sem_getvalue(&is_unit_full[i], &value);

            if (value == 1)
            { //  Odada test yapılmıyorsa value==1
                sem_getvalue(&seats_of_unit[i], &value);
                printf("\t#\tUnit-%d\n", (i + 1));
            }
            else if (value == 0)
            { //  Odada test başlamışsa value==1
                sem_getvalue(&seats_of_unit[i], &value);
                printf("\t#\tUnit-%d Test Started\n", (i + 1));
            }
        }
        printf("\n");
        if (testedPatient > 0)
        {
            printf("%sTotal tested patient : %s%d%s\n\n", YEL, BLU, testedPatient, RESET);
        }
        int emptySeatCount;
        int testUnitIndex = *(int *)n;

        sem_getvalue(&seats_of_unit[testUnitIndex], &emptySeatCount);

        if (emptySeatCount == 3)
        {
            printf("%sAnnounce:%s Unit-%d room is ventilating\n", ANNOUNCE, RESET, (testUnitIndex + 1));
        }
        else if (emptySeatCount == 2)
        {
            printf("\n");
            printf("\t%sAnnounce:%s Unit-%d has a patient and waits for two more patients.", ANNOUNCE, RESET, (testUnitIndex + 1));
            printf("\n\t%sWARNING:%sPlease, pay attention to your social distance and hygiene; use a mask!\n", WARNING, RESET);
        }
        else if (emptySeatCount == 1)
        {
            printf("\n");
            printf("\t%sAnnounce:%s Unit-%d has two patients. Need last patient to start Covid-19 test\n", ANNOUNCE, RESET, (testUnitIndex + 1));
        }
        else if (emptySeatCount == 0)
        {

            printf("\n\t%sAnnounce:%s Unit-%d is full and test is starting\n", ANNOUNCE, RESET, (testUnitIndex + 1));

            sem_wait(&is_unit_full[testUnitIndex]); // Test odasında test başladı.
            sleep(rndValue(TEST_TIME));             //test yapılması için geçerli olacak olan süre.
            for (i = 0; i < UNIT_SIZE; i++)
            {                                            //Test sonrası odadaki koltuklar için
                sem_post(&seats_of_unit[testedPatient]); //koltukları boş oolarak işaretliyor
            }

            testedPatient += ROOM_SIZE;
            printf("\n\t%sAnnounce:%s Test finished in %sUnit-%d%s and %scleaning started!%s\n", ANNOUNCE, RESET, WARNING, (testUnitIndex + 1), RESET, WARNING, RESET);
            sem_post(&order_mtx);
            sem_post(&is_unit_full[testUnitIndex]);
        }

        sem_post(&order_mtx);
    }
}
