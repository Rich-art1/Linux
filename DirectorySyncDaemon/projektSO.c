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

//funkcja wczytuj¹ca numer sygna³u (SIGUSR1 ma nr.10)
void sigma(int signalNumber)
{
	sig = signalNumber;
}

//funkcja zapisuj¹ca logi
void logs(char* action, char* string1, char* string2)
{
	//zarezerwowana tablica
	char napis[99];
	//otwieramy log-a, podaj¹c process id, oraz usera
	openlog("projekt", LOG_PID, LOG_FTP);
	//³¹czymy tablice charów w jedn¹, aby przekazaæ j¹ do logów
	sprintf(napis, "%s %s %s", action, string1, string2);
	//zapisujemy nasz komunikat (komunikat krytyczny ustawi³em dlatego, gdy¿ przy zwyk³ym komunikacie informacyjnym czasami pojawia³y siê problemy)
	syslog(LOG_CRIT, "%s", napis);
	//zamykamy log-a
	closelog();
}

//demon idzie spac
void GoSleep(int sleepTime)
{
	signal(SIGUSR1, sigma);//wczytanie sygna³u

	//zapisujemy inforamcjê o pójœciu spaæ do logów
	logs("Sleep", "", "");
	//wywo³ujemy funkcjê sleep
	sleep(sleepTime);
	//sprawdzamy czy otrzymaliœmy jakiœ sygna³
	if (sig != 0)
	{
		//zapisujemy inforamcjê o otrzymaniu sygna³u SIGUSR1 do logów
		logs("Obudzony przez SIGUSR1", "", "");

		printf("\n \n Program wybudzony przy pomocy SIGUSR1. \n \n");
		sig = 0; //ustawiamy wartoœæ z powrotem na 0
	}
	else
	{
		//zapisujemy inforamcjê o naturalnym wybudzeniu do logów
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
	int size_Path = strlen(Path); //wyznaczamy d³ugoœæ œcie¿ki
	int size_fileName = strlen(fileName); //wyznaczamy d³ugoœæ nazwy pliku

	//rezerwujemy miejsce w pamiêci na nasz¹ now¹ tablicê
	char* wynik = malloc(sizeof(char) * (size_Path + size_fileName + 3));

	int size = size_Path + size_fileName;//suma wielkoœci tablic

	int i;
	//podstawiamy wartoœci ze œcie¿ki
	for (i = 0; i < size_Path; i++)
	{
		wynik[i] = Path[i];
	}
	//dok³adamy '/' aby œcie¿ka siê zgadza³a
	wynik[size_Path] = '/';
	size_Path += 1;//zwiêkszamy o 1 z powodu dodania '/'
	//podstawiamy wartoœci z nazwy pliku
	for (i = 0; i < size_fileName; i++)
	{
		wynik[size_Path + i] = fileName[i];
	}
	//zwracamy jako pojedyncz¹ tablicê char-ów
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

	//zwalniamy pamiec któr¹ wczeœniej zarezerwowaliœmy
	munmap(source, fileSize);
	munmap(destination, fileSize);

	//zamykamy pliki
	close(sourceFile);
	close(destinationFile);
}

//kopiuje male pliki
void copy(char* sourcePathName, char* destinationPathName)
{
	//ustawienia pozwalajace na bezproblemowe otwarcie powsta³ych plików
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
	//zmienn¹ flag wykorzystujemy by rozpoznaæ czy dany plik wyst¹pi³ w folderze docelowym
	int flag = 0;

	//otwieramy plik Ÿród³owy, oraz tworzymy zmienne które pozwol¹ nam wczytywaæ informacje o plikach
	DIR* source;
	struct dirent* dir_source;
	source = opendir(sourcePathName);
	struct stat attr_source;

	//przegl¹damy wszystkie pliki w wskazanym folderze
	while ((dir_source = readdir(source)) != NULL)
	{
		flag = 0; //wraz z przejœciem do kolejnego pliku zerujemy flagê
		time_source = 0;

		int fileSize = attr_source.st_size; //wczytujemy wielkoœæ pliku Ÿród³owego

		time_source = attr_source.st_mtime; //wczytujemy czas ostatniej modyfikacji pliku Ÿród³owego

		//zmienne pozwalaj¹ce nam siê poruszaæ po plikach w folderze docelowym
		DIR* destination;
		struct dirent* dir_destination;
		destination = opendir(destinationPathName);//otwieramy folder docelowy
		//przegl¹damy wszystkei pliki znajduj¹ce siê w pliku docelowym
		while ((dir_destination = readdir(destination)) != NULL)
		{
			struct stat attr_destination;
			stat(fullname(destinationPathName, dir_destination->d_name), &attr_destination); //wczytujemy statystyki pliku docelowego
			stat(fullname(sourcePathName, dir_source->d_name), &attr_source); //wczytujemy statystyki pliku Ÿród³owego

			time_destination = attr_destination.st_mtime; //wczytujemy czas modyfikacji pliku docelowego
			time_source = attr_source.st_mtime;	//wczytujemy czas modyfikacji pliku Ÿród³owego

			if (strcmp(dir_source->d_name, dir_destination->d_name) == 0) //sprawdzamy, czy pliki maj¹ tak¹ sam¹ nazwê
			{
				flag = 1; //jesli tak to oznaczamy flag¹, ¿e uda³o siê nam znaleŸæ plik o danej nazwie
				if (dir_source->d_type == DT_REG) //sprawdzamy, czy plik Ÿród³owy jest typu regularnego
				{
					if (time_destination >= time_source)//sprawdzamy, czy czas modyfikacji pliku docelowego nie jest przypadkiem póŸniejszy ni¿ pliku Ÿród³owego
					{
						printf("\n No need to synch \n file %s is newer than %s \n\n", fullname(destinationPathName, dir_destination->d_name), fullname(sourcePathName, dir_source->d_name));
					}
					else
					{
						//zapisujemy do logów informacje o tym jaki plik jest kopiowany, oraz œcie¿ka pod któr¹ zostanie zapisany
						logs("Synced", fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));

						printf("Synching file: \n %s with %s \n\n", fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));

						//do testów
						//printf("\n \n %ld - %s \n    %ld  -  %s \n \n",time_destination,fullname(destinationPathName,dir_destination->d_name),time_source, fullname(sourcePathName,dir_source->d_name));
						if (attr_source.st_size <= 64) //sprawdzamy, czy wielkoœæ pliku nie jest wiêksza ni¿ 64 bajty
						{
							//jesli nie, u¿ywamy kopiowania, którego nauczyliœmy siê podczas zajêæ
							copy(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));
						}
						else
						{
							//jeœli mamy do czynienia z plikiem o wielkoœci wiêkszej ni¿ 64 bajty to u¿ywamy mapowania do pamiêci
							copyBigFile(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));
						}
					}
				}
				//sprawdzamy, czy program powinien braæ pod uwagê katalogi
				if (spr == 1)
				{	//upewniamy siê, ¿e nie wczytamy odniesieñ do obecnego katalogu, ani te¿ katalogu nad obecnym katalogiem
					if (!strcmp(dir_destination->d_name, ".") || !strcmp(dir_destination->d_name, "..") || !strcmp(dir_source->d_name, ".") || !strcmp(dir_source->d_name, ".."))
					{
						break; //jesli mamy do czynienia z jednym z nich to ignorujemy ten przypadek
					}
					else
					{
						//sprawdzamy czy pliki o tych samych nazwach na pewno s¹ katalogami
						if (dir_destination->d_type == DT_DIR && dir_source->d_type == DT_DIR)
						{
							//je¿eli tak, to wywo³ujemy tê sam¹ funkcjê w której siê znajdujemy, wraz z zmienionymi œcie¿kami
							search(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_destination->d_name), 1);
							break;
						}
					}
				}
			}
		}
		//jeœli flaga jest równa 0 to oznacza, ¿e nie znaleŸliœmy odpowiednika w katalogu docelowym i sprawdzamy, czy nasz plik jest typu regularnego
		if (flag == 0 && dir_source->d_type == DT_REG)
		{
			//zapisujemy do logów informacjê o utworzeniu pliku oraz jego œcie¿ki
			logs("Created file", fullname(destinationPathName, dir_source->d_name), "");

			printf("Created file: \n %s based on %s \n\n", fullname(destinationPathName, dir_source->d_name), fullname(sourcePathName, dir_source->d_name));

			//wybieramy rodzaj kopiowania na podstawie wielkoœci pliku
			if (attr_source.st_size <= 64)
			{
				copy(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));
			}
			else
			{
				copyBigFile(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name));
			}
		}
		//sprawdzamy, czy flaga jest równa 0, czy nasz plik Ÿród³owy jest katalogiem, oraz czy mamy braæ pod uwagê katalogi
		if (flag == 0 && dir_source->d_type == DT_DIR && spr == 1)
		{
			//upewaniamy siê, ¿e nie weŸmiemy pod uwagê odniesieñ do obecnego katalogu oraz katalogu u jeden nad naszym
			if (!strcmp(dir_source->d_name, ".") || !strcmp(dir_source->d_name, ".."))
			{
			}
			else
			{
				//zapisujemy informacjê o utworzeniu katalogu do logów, wraz ze œcie¿k¹
				logs("Created directory", fullname(destinationPathName, dir_source->d_name), "");

				printf("Created directory: \n %s based on %s \n\n", fullname(destinationPathName, dir_source->d_name), fullname(sourcePathName, dir_source->d_name));
				//tworzymy katalog za pomoc¹ mkdir, oraz nadajemy mu uprawnienia S_IRWXU
				mkdir(fullname(destinationPathName, dir_source->d_name), S_IRWXU);
				//wywo³ujemy funkcjê w której siê znajdujemy na wypadek, gdyby w obecnie badanym kataolgu Ÿród³owym znajdowa³y siê jeszcze inne pliki
				search(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_source->d_name), 1);
			}
		}
		//zamykamy plik docelwoy
		closedir(destination);
	}
	//zamykamy plik Ÿród³owy
	closedir(source);
}


//funkcja do usuwania katalogów(w razie gdyby nie by³y puste
void deleteDirectory(char* pathName)
{
	//zmienne pozwalaj¹ce nam siê poruszaæ po plikach
	DIR* directory;
	struct dirent* dir;
	directory = opendir(pathName);
	//przechodzimy po wszystkich plikach
	while ((dir = readdir(directory)) != NULL)
	{
		//sprawdzamy czy obecny plik jest katalogiem
		if (dir->d_type == DT_DIR)
		{
			//upewniamy siê ¿e nie jest to odniesienie
			if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
			{
			}
			else
			{
				//je¿eli nie to wywo³ujemy funkcjê w której siê znajdujemy, gdy¿ nie mamy pewnoœci, czy w œrodku nie znajduj¹ siê kolejne pliki
				deleteDirectory(fullname(pathName, dir->d_name));
			}
		}
		//je¿eli plik nie jest katalogiem to go po prostu usuwamy
		remove(fullname(pathName, dir->d_name));
	}
}

//wyszukujemy i usuwamy pliki ktorych nie ma w zródle
void search_delete(char* sourcePathName, char* destinationPathName, int spr)
{
	//zmienna pozwalaj¹ca nam siê poruszaæ po plikach
	DIR* destination;
	struct dirent* dir_destination;
	//otwieramy katalog docelowy
	destination = opendir(destinationPathName);
	//flaga pomo¿e nam oznaczyæ, czy napotkaliœmy plik o danej nazwie
	int flag = 0;

	//przechodzi przez wszystkie pliki z katalogu docelowego
	while ((dir_destination = readdir(destination)) != NULL)
	{
		flag = 0;//utawiamy wartoœæ flagi na 0
		//sprawdzamy czy obecny plik jest zwyk³ym plikiem, b¹dŸ te¿, czy jest on katalogiem i czy mieliœmy braæ je pod uwagê
		if (dir_destination->d_type == DT_REG || (dir_destination->d_type == DT_DIR && spr == 1))
		{
			//zmienna pozwalaj¹ce nam siê poruszaæ po plikach
			DIR* source;
			struct dirent* dir_source;
			//otwieramy katalog Ÿród³owy
			source = opendir(sourcePathName);

			//przechodzi przez wszystkie pliki w katalogu Ÿród³owym
			while ((dir_source = readdir(source)) != NULL)
			{
				//sprawdzamy czy nazwy obecnie przegl¹danych plików siê zgadza
				if (strcmp(dir_source->d_name, dir_destination->d_name) == 0)
				{
					flag = 1; //znaleŸliœmy plik o danej nazwie, wiêc nie bêdziemy musieli go usuwaæ

					//sprawdzamy czy dany plik nie jest odniesieniem do katalogu
					if (!strcmp(dir_destination->d_name, ".") || !strcmp(dir_destination->d_name, "..") || !strcmp(dir_source->d_name, ".") || !strcmp(dir_source->d_name, ".."))
					{

					}
					else
					{	//sprawdzamy czay dany plik docelowy jest katalogiem, czy plik Ÿród³owy o tej samej nazwie jest katalogiem, oraz czy mieliœmy braæ katalogi pod uwagê
						if (dir_destination->d_type == DT_DIR && dir_source->d_type == DT_DIR && spr == 1)
						{
							//wywo³ujemy funkcjê w której siê znajdujemy, wraz z zmodyfikowanymi œcie¿kami, by porównaæ zawartoœæ koljnego katalogu
							search_delete(fullname(sourcePathName, dir_source->d_name), fullname(destinationPathName, dir_destination->d_name), 1);
						}
					}
					break;
				}
			}
			//jeœli flaga=0 oznacza to, ¿e nie ma takiego pliku w katalogu docelowym
			if (flag == 0)
			{
				//zapisujemy informacjê o usuniêciu pliku w logach, wraz ze œcie¿k¹
				printf("\n Deleted: \n %s no equivalent in source directory\n \n", fullname(destinationPathName, dir_destination->d_name));

				logs("Deleted", fullname(destinationPathName, dir_destination->d_name), "");


				//sprawdzamy, czy nasz plik jest katalogem, oraz czy mieliœmy je braæ pod uwagê
				if (dir_destination->d_type == DT_DIR && spr == 1)
				{
					//wywo³ujemy funkcjê do usuwania zawartoœci katalogów, gdy¿ gdyby coœ siê w nim znajdowa³o to funkcja remove() by nie zadzia³a³a
					deleteDirectory(fullname(destinationPathName, dir_destination->d_name));
				}
				//usuwamy plik któremu obecnie siê przygl¹daliœmy
				remove(fullname(destinationPathName, dir_destination->d_name));
			}
			//zamykamy katalog Ÿród³owy
			closedir(source);
		}
	}
	//zamykamy katalog docelowy
	closedir(destination);
}



int main(int argc, char** argv)
{
	int sleepTime = 300;


	FILE* plik;//plik Ÿród³owy
	FILE* plik2;//plik docelowy

	//spr=1  katalogi tez usuwa, spr=0 katalogi zostawia w spokoju
	int spr = 0;

	//wczytujemy 3 argumenty
	char* pathName1 = argv[1];
	char* pathName2 = argv[2];
	char* directory = argv[3];

	//sprawdzamy czy opcjonalny warunek -R zosta³ podany, jeœli tak to ustawiamy wartoœæ spr na 1
	if (directory != NULL)
	{
		if (strcmp(directory, "-R") == 0)
		{
			spr = 1;
		}
	}
	//sprawdzamy czy obie œcie¿ki wskazuj¹ na katalogi
	if (CheckDir(pathName1) == 1 && CheckDir(pathName2) == 1)
	{
		printf("Obie sciezki prowadza do katalogow, zasypiam. \n \n");
		while (1)
		{
			pid_t processID;
			processID = fork(); //forkujemy proces

			//sprawdamy czy siê uda³o, jeœli tak to mo¿emy wyjœæ z rodzica
			if (processID < 0)
			{
				exit(EXIT_FAILURE);
			}
			if (processID > 0)
			{
				exit(EXIT_SUCCESS);
			}

			//wywo³ujemy poszczególne funkcje
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
		printf("Conajmniej jedna ze œcie¿ek jest nieprawid³owa!\n");
		return 0;
	}
}
