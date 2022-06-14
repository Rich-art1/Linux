#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_CORES 1

pthread_mutex_t mutex= PTHREAD_MUTEX_INITIALIZER;


int step = 1;

int N;
int A;
int B;
int out_a;
int out_b;

struct args{
int car_no;
int town; //1=A 0=B
};


void wypisz(int nr_car, int kierunek) //kierunek=1 dla A->B, natomiast else to B->A
{
    printf("Przejazd nr.%d\n______________________ \nKierunek: ",step); //wypisujemy ktory to przejazd
    step++; //zwiekszamy ilosc przejazdu
    
    if(kierunek==1)
    {
    printf("A->B \n");
    out_a=out_a-1;                                                               				 	//zmniejszamy ilosc samochodow w kolejce z miasta A, gdyz samochod przejezdza przez most
    printf("\nZmiana \nA-%d %d>>>[>>%d>>]<<<%d %d-B \n",A,out_a,nr_car,out_b,B);  					//wypisujemy zmiane
    B++; 																							//zwiekszamy ilosc samochodow w miescie B, gdyz przyjechal samochod z miasta A
    printf("\nNowy stan:\n A: %d kolejka: %d \n B: %d kolejka: %d \n______________________ \n\n\n", A, out_a, B, out_b); //aktualny stan
    }
    else
    {
    printf("B->A \n");
    out_b=out_b-1; 																					//zmniejszamy ilosc samochodow w kolejce z miasta B, gdyz samochod przejezdza przez most
    printf("\nZmiana: \nA-%d %d>>>[<<%d<<]<<<%d %d-B \n",A,out_a,nr_car,out_b,B); 					//wypisujemy zmiane
    A++; 																							//zwiekszamy ilosc samochodow w miescie A, gdyz przyjechal samochod z miasta B
    printf("\nNowy stan:\n A: %d kolejka: %d \n B: %d  kolejka: %d \n______________________\n\n\n", A, out_a, B, out_b); //aktualny stan
    }
}


void *queue(void *arguments)
{
    pthread_mutex_lock(&mutex); //blokujemy mutexa
    
    struct args *argument=(struct args *)arguments; //wczytujemy wartosc z argumentu do zmiennej
    
    wypisz(argument->car_no, argument->town); //wywolujemy funkcje zmieniajace wartosci, oraz wypisujaca informacje

    pthread_mutex_unlock(&mutex); //odblokowujemy mutexa

    pthread_exit(NULL); //wychodzimy z watku
}

int main(int argc, char *argv[])
{
	N=atoi(argv[1]);
    srand(time(NULL));

    //losujemy ilosc samochodow w miescie A
    A=rand()%N+0;

    //reszte samochodow przypisujemy do miasta B
    B=N-A;

    //wydzielamy z miasta A ilosc samochodow chcacych wyjechac z niego (out_a)
    out_a=rand()%A+0;
    A=A-out_a;

    //wydzielamy z miasta B ilosc samochodow chcacych wyjechac z niego (out_b)
    out_b=rand()%B+0;
    B=B-out_b;

    //wypisujemy wylosowane dane
    printf("\n \n Ilosc w A: %d  kolejka: %d", A, out_a);
    printf("\n \n Ilosc w B: %d  kolejka: %d \n\n\n\n", B, out_b);

    pthread_t *car_thread=malloc(sizeof(pthread_t)*MAX_CORES*N);

    pthread_mutex_init(&mutex, NULL); //inicjalizujemy mutexa

    struct args samochody[N];


    int miasto=1; //1=A  0=B
    
   //przydzielamy numery samochodom w miescie A
    for(int i=0;i<A+out_a;i++)
    {
    samochody[i].car_no=i;
    samochody[i].town=miasto;
    }
    
    //uzupelniamy wartosci dla samochodow z miasta B
    miasto=0;
    for(int i=A+out_a; i<N;i++)
    {
    samochody[i].car_no=i;
    samochody[i].town=miasto;
    }
    
int temp=0; //zmienna trzymajaca wartosc z numerem samochodu
int temp2=out_a+out_b; //suma samochodow ktore chca przejechac przez most
int temp3=out_a; //suma samochodow ktore chca przejechaz z miasta A

	//tworzymy samochody
    for(int i = 0; i < temp2; i++)
    {
    if(i==temp3) //sprawdzamy, czy to juz samochody z B
    {
    temp=temp+A;
    }
    	//tworzymy watek
        if(pthread_create(&car_thread[i], NULL, &queue, (void *)&samochody[temp])!=0)
        {
        perror("Create error:  ");
        }
        
        temp++; 
    }
    //joinujemy
    for(int i=0;i<temp2;i++)
    {
    if(pthread_join(car_thread[i], NULL)!=0)
        {
        perror("Join error:  ");
        }
    }
    pthread_mutex_destroy(&mutex); //usuwamy mutexa
}
