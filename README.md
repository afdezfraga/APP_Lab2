# APP_Lab2

## Introducción

En esta memoria se describe el proceso de paralelización con colectivas no bloqueantes de los códigos `pi_integral.c`, `dotprodl.c`, `mxvnm.c` y `sqrt.c`. 

Para cada código, en primer lugar se explicarán los cambios llevados a cabo para usar estas colectivas no bloqueantes, y finalmente, se comparará el rendimiento de las dos versiones para comparar el rendimiento de los patrones de comunicación.

## pi_integral.c

### Cambios
-----
El código paralelo cuenta con tres colectivas:

 - Un `MPI_Bcast` de la cantidad de datos a usar para la estimación. Esta comunicación se lleva a cabo al principio, para que cada proceso sepa sobre que rango ejecutar sus computos.
 - Un `MPI_Reduce` de la estimación de pi local de cada proceso a la estimación global de la simulación.
 - Un `MPI_Reduce` de la duración de la simulación para una correcta medición de tiempos.

En los tres casos, el dato a comunicar es necesario justo después de ser obtenido, por lo tanto no podemos solapar ningún computo con estas comunicaciones. El resultado es que las tres comunicaciones son reemplazadas por sus versiones asincronas (`MPI_Ibcast`, `MPI_Ireduce`), y justo después de cada una se ejecuta un `MPI_Wait` para asegurar que ya hemos recibido el dato y podemos trabajar con él.

### Evaluación del rendimiento
-----

| .....        | 1 Process | 2 Processes | 4 Processes | 8 Processes | 16 Processes |
| :-----------:|----------:| -----------:| -----------:| -----------:| ------------:|
| Blocking     | 4.251 s   | 2.146 s     | 1.091 s     | 0.604 s     | 0.305 s      |
| Non Blocking | 4.364 s   | 2.160 s     | 1.092 s     | 0.603 s     | 0.304 s      |

Debido a que no se solapan las comunicaciones con ningún cómputo, no se aprecia ninguna mejora.

## dotprod.c

### Cambios
-----
El código paralelo cuenta con cuatro colectivas:

 - Un `MPI_Bcast` de la cantidad de datos a usar para la estimación. Esta comunicación se lleva a cabo al principio, para que cada proceso sepa sobre que rango ejecutar sus computos.
 - Dos `MPI_Scatterv`, para distribuir los dos vectores de entrada entre los procesos.
 - Un `MPI_Reduce` del resultado local de cada proceso al resultado global del programa.

En todos los casos, los datos a comunicar son necesarios justo después de ser obtenidos, por lo tanto no podemos solapar ningún computo con estas comunicaciones. 

Esto podría evitarse en la primera comunicación, pero como la inicialización de los vectores es un poco "de juguete", no se ha considerado como un cómputo real del programa que solapar con las comunicaciones. 

Esto quiere decir que, en el caso de las comunicaciones `MPI_Bcast` y `MPI_Reduce` son reemplazadas por sus versiones asincronas (`MPI_Ibcast` y `MPI_Ireduce`), y justo después de cada una se ejecuta un `MPI_Wait` para asegurar que ya hemos recibido el dato y podemos trabajar con él.

Sin embargo, respecto a los vectores que distribuimos con `MPI_Scatterv`, ambos son necesarios para empezar la fase de computo, y por lo tanto, no es necesario esperar una vez por comunicación, sino que solo es necesario bloquear una vez, para asegurarnos que ambos vectores se han distribuido. Por esto se han sustituido ambas rutinas por la versión asincrona (`MPI_Iscatterv`) y a continuación esperamos a que ambas se completen con `MPI_Waitall`. 

### Evaluación del rendimiento
-----

```sh
srun dotprod_MPI_Sync 500.000.000
```

| .....        | 1 Process | 2 Processes | 4 Processes | 8 Processes | 16 Processes |
| :-----------:|----------:| -----------:| -----------:| -----------:| ------------:|
| Blocking     | 8.181 s   | 7.026 s     | 7.042 s     | 7.240 s     | 7.246 s      |
| Non Blocking | 8.331 s   | 6.680 s     | 5.664 s     | 4.885 s     | 4.679 s      |

En este caso el cuello de botella no es encuentra en el computo, sino que lo encontramos en otras partes del programa, (inicialización sequencial de los vectores, reserva de memoria, comunicación de los datos...). Por ello, en este caso, el cambio en el patrón de comunicaciones permite que el programa obtenga un mejor rendimiento.

## mxvnm.c

### Cambios
-----
El código paralelo se puede dividir en 3 fases:

 - Fase de distribución de datos
 - Fase de computo.
 - Fase de recolección de datos.

Tanto en la primera como en la última fase existen operaciones colectivas que queremos modificar. Trataremso cada una de estas fases por separado.

#### 1.- Fase de distribución de datos

Durante esta fase queremos distribuir 4 datos.

 - `MPI_Bcast` a la dimensión M de la matriz y del vector.
 - `MPI_Bcast` a la dimensión N de la matriz.
 - `MPI_Bcast` al vector.
 - `MPI_Scatterv` a las filas de la matriz. 

 El primer dato lo necesitamos de inmediato para reservar el espacio del vector antes de la tercera comunicación. Por lo tanto lo substituimos por su versión no bloqueante (`MPI_Ibcast`), pero ejecutamos un `MPI_Wait` practicamente después.

 Respecto al segundo dato, no lo necesitamos hasta que empecemos a preparar la última comunicación, por lo tanto podemos solapar este mensaje con el malloc del vector.

 Respecto a los dos últimos mensajes, necesitaremos esos datos el comienzo de la fase de cómputo, por lo tanto:

 1. Iniciamos la versión no bloqueante para distribuir el vector (`MPI_Ibcast`)
 2. Preparamos la distribución de la matriz
 3. Iniciamos la versión no bloqueante de distribución de esa matriz (`MPI_Iscatterv`)
 4. Nos aseguramos que ambos datos estan disponibles antes de empezar la fase de computo con `MPI_Waitall`.

#### 2.- Fase de recolección de datos

  En esta fase se recolecta el vector de salida en el proceso principal usando `MPI_Gatherv`. Los datos son necesarios justo despues de ser recolectados, así que la colectiva se substituye por su versión no bloqueante (`MPI_Igatherv`) y juste después nos aseguramos que los datos estan disponibles con `MPI_Wait`.

En los tres casos, el dato a comunicar es necesario justo después de ser obtenido, por lo tanto no podemos solapar ningún computo con estas comunicaciones. El resultado es que las tres comunicaciones son reemplazadas por sus versiones asincronas (`MPI_Ibcast`, `MPI_Ireduce`), y justo después de cada una se ejecuta un `MPI_Wait` para asegurar que ya hemos recibido el dato y podemos trabajar con él.

### Evaluación del rendimiento
-----

```sh
srun mxvnm_MPI_Sync 32568 32568
```

| .....        | 1 Process | 2 Processes | 4 Processes | 8 Processes | 16 Processes |
| :-----------:|----------:| -----------:| -----------:| -----------:| ------------:|
| Blocking     | 13.183 s  | 11.195 s    | 10.396 s    | 10.975 s    | 10.951 s     |
| Non Blocking | 13.135 s  | 11.376 s    | 9.571 s     | 8.883 s     | 8.367 s      |

Igual que en el caso anterior el cuello de botella no se encuentra en el computo, sino que lo encontramos en otras partes del programa, (inicialización sequencial de los vectores, reserva de memoria, comunicación de los datos...). Por ello, igual que en el caso anterior, el cambio en el patrón de comunicaciones permite que el programa obtenga un mejor rendimiento.


## sqrt.c

### Cambios
-----
El código paralelo se ha modificado de forma que, en lugar de realizar todos los cómputos y despues comunicar el resultado, esta se va haciendo en varios pasos. Es decir, en cada paso se calcula un bloque de datos, se comunica de forma no bloqueante esos datos, y se pasa al siguiente paso. Solo una vez que todos los pasos se han completado, esperamos a que todos los datos esten disponibles en el proceso principal.

Para esto se ha añadido un parametro adicional al programa de forma que su uso queda como: 

```sh
$ ./sqrt_pipeline 
    Usage: avg num_elements_per_proc num_steps
```

Donde el nuevo parámetro indica en cuantos pasos se realizará el cómputo y las comunicaciones.

### Evaluación del rendimiento
-----

La evaluación del rendimiento se hara para 4 procesos y 500.000.000 elementos por proceso.

```sh
srun sqrt_pipeline 500000000 num_steps
```

| .....        | 1 Step    | 10 Steps    | 100 Steps   | 500 Steps   | 1.000 Steps | 2.000 Steps | 10.000 Steps |
| :-----------:|----------:| -----------:| -----------:| -----------:| -----------:| ------------:| ------------:|
|sqrt_pipeline | 7.124 s   | 6.860 s     | 6.773 s     | 6.680 s     | 6.606 s     | 6.716 s      | 6.732 s      |


Se puede observar que los tiempos mejoran hasta aproximadamente las 1.000 iteraciones, y a partir de ahí el rendimiento vuelve a degradarse.

Es decir, el punto óptimo para 4 procesos se encuentra aproximadamente cuando cada proceso computa y comunica bloques de 500.000 elementos.