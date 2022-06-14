Przy uruchomieniu programu podajemy jako argument wartość naturalną N która oznacza ilość wszystkich samochodów.

Następnie losowane są wartości ile samochodów znajduje się w mieście A oraz B. 

Po czym z tych wartości losujemy ile z nich wyjeżdża z miasta 
(te samochody ustawiają się do kolejki przed mostem, więc nie liczą się do sumy żadnego z miast do póki nie przejadą mostu).

Następnie tworzymy oraz joinujemy wątki, które zawierają zablokowanie mutexa na czas wywołania, oraz odblokowanie po wykonaniu się.
W czasie blokady wywołana zostaje funkcja która wypisuje zmianę która zachodzi, 
oraz zmieniane są wartości zmiennych globalnych reprezentujące ilość samochodów w konkretnych miejscach
