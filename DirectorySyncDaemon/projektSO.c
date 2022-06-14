#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <syslog.h>

static volatile sig_atomic_t sig = 0;

//funkcja wczytuj�ca numer sygna�u (SIGUSR1 ma nr.10)
void sigma(int signalNumber)
{
	sig = signalNumber;
}

//funkcja zapisuj�ca logi
void logs(char* action, char* string1, char* string2)
{
	//zarezerwowana tablica
	char napis[99];
	//otwieramy log-a, podaj�c process id, oraz usera
	openlog("projekt", LOG_PID, LOG_FTP);
	//��czymy tablice char�w w jedn�, aby przekaza� j� do log�w
	sprintf(napis, "%s %s %s", action, string1, string2);
	//zapisujemy nasz komunikat (komunikat krytyczny ustawi�em dlatego, gdy� przy zwyk�ym komunikacie informacyjnym czasami pojawia�y si� problemy)
	syslog(LOG_CRIT, "%s", napis);
	//zamykamy log-a
	closelog();
}

//demon idzie spac
void GoSleep(int sleepTime)
{
	signal(SIGUSR1, sigma);//wczytanie sygna�u

	//zapisujemy inforamcj� o p�j�ciu spa� do log�w
	logs("Sleep", "", "");
	//wywo�ujemy funkcj� sleep
	sleep(sleepTime);
	//sprawdzamy czy otrzymali�my jaki� sygna�
	if (sig != 0)
	{
		//zapisujemy inforamcj� o otrzymaniu sygna�u SIGUSR1 do log�w
		logs("Obudzony przez SIGUSR1", "", "");

		printf("\n \n Program wybudzony przy pomocy SIGUSR1. \n \n");
		sig = 0; //ustawiamy warto�� z powrotem na 0
	}
	else
	{
		//zapisujemy inforamcj� o naturalnym wybudzeniu do log�w
		logs("Program wybudzony naturalnie", "", "");

		printf("\n \n Program wybudzony naturalnie. \n \n");
	}
}

//sprawdzamy czy nie jest folderem
int CheckDir(char* pathName)
{
	struct stat sb;

	if (stat(pathName, &sb) == 0 && S_ISDIR(sb.st_mode))
		return 1;
	else
		return 0;

}

//laczenie sciezki do folderu i nazwy pliku
char* fullname(char* Path, char* fileName)
{
	int size_Path = strlen(Path); //wyznaczamy d�ugo�� �cie�ki
	int size_fileName = strlen(fileName); //wyznaczamy d�ugo�� nazwy pliku

	//rezerwujemy miejsce w pami�ci na nasz� now� tablic�
	char* wynik = malloc(sizeof(char) * (size_Path + size_fileName + 3));

	int size = size_Path + size_fileName;//suma wielko�ci tablic

	int i;
	//podstawiamy warto�ci ze �cie�ki
	for (i = 0; i < size_Path; i++)
	{
		wynik[i] = Path[i];
	}
	//dok�adamy '/' aby �cie�ka si� zgadza�a
	wynik[size_Path] = '/';
	size_Path += 1;//zwi�kszamy o 1 z powodu dodania '/'
	//podstawiamy warto�ci z nazwy pliku
	for (i = 0; i < size_fileName; i++)
	{
		wynik[size_Path + i] = fileName[i];
	}
	//zwracamy jako pojedyncz� tablic� char-�w
	return wynik;
}

//kopiuje duze pliki
void copyBigFile(char* sourcePathName, char* destinationPathName) {

	int sourceFile;
	int destinationFile;
	char* source;
	char* destination;
	size_t fileSize;

	sourceFile = open(sourcePathName, O_RDONLY);							//otwieramy plik zrodlowy
	fileSize = lseek(sourceFile, 0, SEEK_END);								//ustalamy wielkosc pliku
	source = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, sourceFile, 0);	//mapujemy plik zrodlowy do pamieci

	destinationFile = open(destinationPathName, O_RDWR | O_CREAT, 0666);	//otwieramy plik docelowy
	ftruncate(destinationFile, fileSize);									//przycinamy plik docelowy do wielkosci zrodlowego

	destination = mmap(NULL, fileSize, PROT_READ | PROT_WRITE, MAP_SHARED, destinationFile, 0); //tworzymy miejsce na plik docelowy

	memcpy(destination, source, fileSize);	//kopiujemy kawalek pamieci (do ktorej wczytalismy plik zrodlowy) do pamieci zarezerwowanej dla pliku docelowego

	//zwalniamy pamiec kt�r� wcze�niej zarezerwowali�my
	munmap(source, fileSize);
	munmap(destination, fileSize);

	//zamykamy pliki
	close(sourceFile);
	close(destinationFile);
}

//kopiuje male pliki
void copy(char* sourcePathName, char* destinationPathName)
{
	//ustawienia pozwalajace na bezproblemowe otwarcie powsta�ych plik�w
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	//otwieramy pliki
	int s_file = open(sourcePathName, O_RDONLY);
	int d_file = creat(destinationPathName, mode);

	//ustawiamy wielkosc bufora
	int liczba = 1;
	char buffer[liczba];

	//kopiujemy pliki w paczkach o konkretnym rozmiarze
	while (read(s_file, buffer, liczba))
	{
		write(d_file, buffer, liczba);
	}

	//zamykamy pliki
	close(d_file);
	close(s_file);

	return;
}

//wyszukuje plikow ktore trzeba skopiowac
void search(char* sourcePathName, char* destinationPathName, int spr)
{
	time_t time_destination;
	time_t time_source;
	//zmienn� flag wykorzystujemy by rozpozna� czy dany plik wyst�pi� w folderze docelowym
	int flag = 0;

	//otwieramy plik �r�d�owy, oraz tworzymy zmienne kt�re pozwol� nam wczytywa� informacje o plikach
	DIR* source;
	struct dirent* dir_source;
	source = opendir(sourcePathName);
	struct stat attr_source;

	//przegl�damy wszystkie pliki w wskazanym folderze
	while ((dir_source = readdir(source)) != NULL)
	{
		flag = 0; //wraz z przej�ciem do kolejnego pliku zerujemy flag�
		time_source = 0;

		int fileSize = attr_source.st_size; //wczytujemy wielko�� pliku �r�d�owego

		time_source = attr_source.st_mtime; //wczytujemy czas ostatniej modyfikacji pliku �r�d�owego

		//zmienne pozwalaj�ce nam si� porusza� po plikach w folderze docelowym
		DIR* destination;
		struct dirent* dir_destination;
		destination = opendir(destinationPathName);//otwieramy folder docelowy
		//przegl�damy wszystkei pliki znajduj�ce si� w pliku docelowym
		while ((dir_destination = readdir(destination)) != NULL)
		{
			struct stat attr_destination;
			stat(fullname(destinationPathName, dir_destination->d_name), &attr_destination); //wczytujemy statystyki pliku docelowego
			stat(fullname(sourcePathName, dir_source->d_name), &attr_source); //wczytujemy statystyki pliku �r�d�owego

			time_destination = attr_destination.st_mtime; //wczytujemy czas modyfikacji pliku docelowego
			time_source = attr_source.st_mtime;	//wczytujemy czas modyfikacji pliku �r�d�owego

			if (strcmp(dir_source->d_name, dir_destination->d_name) == 0) //sprawdzamy, czy pliki maj� tak� sam� nazw�
			{
				flag = 1; //jesli tak to oznaczamy flag�, �e uda�o si� nam znale�� plik o danej nazwie
				if (dir_source->d_type == DT_REG) //sprawdzamy, czy plik �r�d�owy jest typu regularnego
				{
					if (time_destination >= time_source)//sprawdzamy, czy czas modyfikacji pliku docelowego nie jest przypadkiem p�niejszy ni� pliku �r�d�owego
					{
						printf("\n No need to synch \n file %s is newer than %s \n\n", fullname(destinationPathName, dir_destination->d_name), fullname(sourcePathName, dir_source->d_name));
					}
					else
					{
						//zapisujemy do log�w informacje o tym jaki plik jest kopiowany, oraz �cie�ka pod kt�r� zostanie zapisany
						logs("Synced", fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));

						printf("Synching file: \n %s with %s \n\n", fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));

						//do test�w
						//printf("\n \n %ld - %s \n    %ld  -  %s \n \n",time_destination,fullname(destinationPathName,dir_destination->d_name),time_source, fullname(sourcePathName,dir_source->d_name));
						if (attr_source.st_size <= 64) //sprawdzamy, czy wielko�� pliku nie jest wi�ksza ni� 64 bajty
						{
							//jesli nie, u�ywamy kopiowania, kt�rego nauczyli�my si� podczas zaj��
							copy(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));
						}
						else
						{
							//je�li mamy do czynienia z plikiem o wielko�ci wi�kszej ni� 64 bajty to u�ywamy mapowania do pami�ci
							copyBigFile(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));
						}
					}
				}
				//sprawdzamy, czy program powinien bra� pod uwag� katalogi
				if (spr == 1)
				{	//upewniamy si�, �e nie wczytamy odniesie� do obecnego katalogu, ani te� katalogu nad obecnym katalogiem
					if (!strcmp(dir_destination->d_name, ".") || !strcmp(dir_destination->d_name, "..") || !strcmp(dir_source->d_name, ".") || !strcmp(dir_source->d_name, ".."))
					{
						break; //jesli mamy do czynienia z jednym z nich to ignorujemy ten przypadek
					}
					else
					{
						//sprawdzamy czy pliki o tych samych nazwach na pewno s� katalogami
						if (dir_destination->d_type == DT_DIR && dir_source->d_type == DT_DIR)
						{
							//je�eli tak, to wywo�ujemy t� sam� funkcj� w kt�rej si� znajdujemy, wraz z zmienionymi �cie�kami
							search(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_destination->d_name), 1);
							break;
						}
					}
				}
			}
		}
		//je�li flaga jest r�wna 0 to oznacza, �e nie znale�li�my odpowiednika w katalogu docelowym i sprawdzamy, czy nasz plik jest typu regularnego
		if (flag == 0 && dir_source->d_type == DT_REG)
		{
			//zapisujemy do log�w informacj� o utworzeniu pliku oraz jego �cie�ki
			logs("Created file", fullname(destinationPathName, dir_source->d_name), "");

			printf("Created file: \n %s based on %s \n\n", fullname(destinationPathName, dir_source->d_name), fullname(sourcePathName, dir_source->d_name));

			//wybieramy rodzaj kopiowania na podstawie wielko�ci pliku
			if (attr_source.st_size <= 64)
			{
				copy(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));
			}
			else
			{
				copyBigFile(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));
			}
		}
		//sprawdzamy, czy flaga jest r�wna 0, czy nasz plik �r�d�owy jest katalogiem, oraz czy mamy bra� pod uwag� katalogi
		if (flag == 0 && dir_source->d_type == DT_DIR && spr == 1)
		{
			//upewaniamy si�, �e nie we�miemy pod uwag� odniesie� do obecnego katalogu oraz katalogu u jeden nad naszym
			if (!strcmp(dir_source->d_name, ".") || !strcmp(dir_source->d_name, ".."))
			{
			}
			else
			{
				//zapisujemy informacj� o utworzeniu katalogu do log�w, wraz ze �cie�k�
				logs("Created directory", fullname(destinationPathName, dir_source->d_name), "");

				printf("Created directory: \n %s based on %s \n\n", fullname(destinationPathName, dir_source->d_name), fullname(sourcePathName, dir_source->d_name));
				//tworzymy katalog za pomoc� mkdir, oraz nadajemy mu uprawnienia S_IRWXU
				mkdir(fullname(destinationPathName, dir_source->d_name), S_IRWXU);
				//wywo�ujemy funkcj� w kt�rej si� znajdujemy na wypadek, gdyby w obecnie badanym kataolgu �r�d�owym znajdowa�y si� jeszcze inne pliki
				search(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name), 1);
			}
		}
		//zamykamy plik docelwoy
		closedir(destination);
	}
	//zamykamy plik �r�d�owy
	closedir(source);
}


//funkcja do usuwania katalog�w(w razie gdyby nie by�y puste
void deleteDirectory(char* pathName)
{
	//zmienne pozwalaj�ce nam si� porusza� po plikach
	DIR* directory;
	struct dirent* dir;
	directory = opendir(pathName);
	//przechodzimy po wszystkich plikach
	while ((dir = readdir(directory)) != NULL)
	{
		//sprawdzamy czy obecny plik jest katalogiem
		if (dir->d_type == DT_DIR)
		{
			//upewniamy si� �e nie jest to odniesienie
			if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
			{
			}
			else
			{
				//je�eli nie to wywo�ujemy funkcj� w kt�rej si� znajdujemy, gdy� nie mamy pewno�ci, czy w �rodku nie znajduj� si� kolejne pliki
				deleteDirectory(fullname(pathName, dir->d_name));
			}
		}
		//je�eli plik nie jest katalogiem to go po prostu usuwamy
		remove(fullname(pathName, dir->d_name));
	}
}

//wyszukujemy i usuwamy pliki ktorych nie ma w zr�dle
void search_delete(char* sourcePathName, char* destinationPathName, int spr)
{
	//zmienna pozwalaj�ca nam si� porusza� po plikach
	DIR* destination;
	struct dirent* dir_destination;
	//otwieramy katalog docelowy
	destination = opendir(destinationPathName);
	//flaga pomo�e nam oznaczy�, czy napotkali�my plik o danej nazwie
	int flag = 0;

	//przechodzi przez wszystkie pliki z katalogu docelowego
	while ((dir_destination = readdir(destination)) != NULL)
	{
		flag = 0;//utawiamy warto�� flagi na 0
		//sprawdzamy czy obecny plik jest zwyk�ym plikiem, b�d� te�, czy jest on katalogiem i czy mieli�my bra� je pod uwag�
		if (dir_destination->d_type == DT_REG || (dir_destination->d_type == DT_DIR && spr == 1))
		{
			//zmienna pozwalaj�ce nam si� porusza� po plikach
			DIR* source;
			struct dirent* dir_source;
			//otwieramy katalog �r�d�owy
			source = opendir(sourcePathName);

			//przechodzi przez wszystkie pliki w katalogu �r�d�owym
			while ((dir_source = readdir(source)) != NULL)
			{
				//sprawdzamy czy nazwy obecnie przegl�danych plik�w si� zgadza
				if (strcmp(dir_source->d_name, dir_destination->d_name) == 0)
				{
					flag = 1; //znale�li�my plik o danej nazwie, wi�c nie b�dziemy musieli go usuwa�

					//sprawdzamy czy dany plik nie jest odniesieniem do katalogu
					if (!strcmp(dir_destination->d_name, ".") || !strcmp(dir_destination->d_name, "..") || !strcmp(dir_source->d_name, ".") || !strcmp(dir_source->d_name, ".."))
					{

					}
					else
					{	//sprawdzamy czay dany plik docelowy jest katalogiem, czy plik �r�d�owy o tej samej nazwie jest katalogiem, oraz czy mieli�my bra� katalogi pod uwag�
						if (dir_destination->d_type == DT_DIR && dir_source->d_type == DT_DIR && spr == 1)
						{
							//wywo�ujemy funkcj� w kt�rej si� znajdujemy, wraz z zmodyfikowanymi �cie�kami, by por�wna� zawarto�� koljnego katalogu
							search_delete(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_destination->d_name), 1);
						}
					}
					break;
				}
			}
			//je�li flaga=0 oznacza to, �e nie ma takiego pliku w katalogu docelowym
			if (flag == 0)
			{
				//zapisujemy informacj� o usuni�ciu pliku w logach, wraz ze �cie�k�
				printf("\n Deleted: \n %s no equivalent in source directory\n \n", fullname(destinationPathName, dir_destination->d_name));

				logs("Deleted", fullname(destinationPathName, dir_destination->d_name), "");


				//sprawdzamy, czy nasz plik jest katalogem, oraz czy mieli�my je bra� pod uwag�
				if (dir_destination->d_type == DT_DIR && spr == 1)
				{
					//wywo�ujemy funkcj� do usuwania zawarto�ci katalog�w, gdy� gdyby co� si� w nim znajdowa�o to funkcja remove() by nie zadzia�a�a
					deleteDirectory(fullname(destinationPathName, dir_destination->d_name));
				}
				//usuwamy plik kt�remu obecnie si� przygl�dali�my
				remove(fullname(destinationPathName, dir_destination->d_name));
			}
			//zamykamy katalog �r�d�owy
			closedir(source);
		}
	}
	//zamykamy katalog docelowy
	closedir(destination);
}



int main(int argc, char** argv)
{
	int sleepTime = 300;


	FILE* plik;//plik �r�d�owy
	FILE* plik2;//plik docelowy

	//spr=1  katalogi tez usuwa, spr=0 katalogi zostawia w spokoju
	int spr = 0;

	//wczytujemy 3 argumenty
	char* pathName1 = argv[1];
	char* pathName2 = argv[2];
	char* directory = argv[3];

	//sprawdzamy czy opcjonalny warunek -R zosta� podany, je�li tak to ustawiamy warto�� spr na 1
	if (directory != NULL)
	{
		if (strcmp(directory, "-R") == 0)
		{
			spr = 1;
		}
	}
	//sprawdzamy czy obie �cie�ki wskazuj� na katalogi
	if (CheckDir(pathName1) == 1 && CheckDir(pathName2) == 1)
	{
		printf("Obie sciezki prowadza do katalogow, zasypiam. \n \n");
		while (1)
		{
			pid_t processID;
			processID = fork(); //forkujemy proces

			//sprawdamy czy si� uda�o, je�li tak to mo�emy wyj�� z rodzica
			if (processID < 0)
			{
				exit(EXIT_FAILURE);
			}
			if (processID > 0)
			{
				exit(EXIT_SUCCESS);
			}

			//wywo�ujemy poszczeg�lne funkcje
			GoSleep(sleepTime);

			search_delete(pathName1, pathName2, spr);

			search(pathName1, pathName2, spr);
		}
		umask(0);

		//nowa sesja dla dziecka
		if (setsid() < 0)
		{
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		printf("Conajmniej jedna ze �cie�ek jest nieprawid�owa!\n");
		return 0;
	}
}
