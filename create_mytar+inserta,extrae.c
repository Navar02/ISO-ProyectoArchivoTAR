/* *
 * * @file create_mytar+inserta,extrae.c
 * * @author ISO-2-G10
 * * @date 24/03/2023
 * * @brief First version of mytar
 * * @details  Create a tar file with only one "data file"
 * *
 * */
/*
FUNCIONALIDAD PARA CREAR E INSERTAR
USO
       inserta_fichero->crea un fichero del tar o inserta en uno.
       targ10 fichero archivo.tar

SINOPSIS
      #include "s_mytarheader.h"
      unsigned long inserta_fichero(int f_mytar, unsigned long tamano, char *filename);

DESCRIPCIÓN

      Si  f_dat es de tipo fichero:

      La función inserta_fichero inserta el fichero f_dat en el fichero f_mytar
      con formato gnu tar de igual manera que la versión previa de **inserta_fichero**:

      Si f_mytar no existe o está vacío, se debe crear el fichero f_mytar y
      se añadirá f_dat tal y como lo realiza la utilidad create_mytar.
      Si f_mytar contiene ficheros con un formato gnu tar correcto, se añadirá el
      fichero fdat con formato gnu tar al final del último bloque de datos del último
      fichero incluido en f_mytar. Esta posición debe ser donde comienzan los dos bloques
      de fin de archivo (con bytes a zero) del fichero f_mytar.
      El formato de f_mytar se describe en la estructura c_header_gnu_tar (ver s_mytarheader.h).

      Si  f_dat es de tipo Directorio:

      Debe introducir primero un elemento de nombre f_dat pero sin datos. Es decir solo se
      añade la información de c_header_gnu_tar correspondiente a f_dat (no se añaden datos).
      Seguido, la función debe introducir en f_mytar todos los fichero regulares
      contenidos en el directorio f_dat (solo en el primer nivel del directorio f_dat)
      Los nombres de los ficheros que se introduzcan tendrán el formato f_dat/xxx donde xxx
      es el nombre de cada fichero regular encontrado.

      Si f_dat es un enlace simbolico:

      Debe introducir primero un elemento de nombre f_dat pero sin datos. Es decir solo se
      añade la información de c_header_gnu_tar correspondiente a f_dat (no se añaden datos).

VALOR DE RETORNO
       Si todo funciona correctamente, inserta_fichero devolverá el número correspondiente
       del último elemento insertado dentro del fichero f_mytar (número de ficheros contenidos
       en f_mytar).
       En caso de error devolverá el correspondiente mensaje y codigo de error solo si no puede abrirlo,
       si no tiene formato tar.

ERRORES
       E_OPEN1     (-1)
           filename hace referencia a un archivo (o directorio) que  no se puede abrir.
       E_OPEN2     (-2)
           No se puede abrir o crear f_mytar.
        E_TARFORM (-3) 
           f_mytar no tiene el formato de gnu tar.

FUNCIONALIDAD DE EXTRAER
NOMBRE
      extrae_fichero->extrae un fichero del tar
      targ10 fichero archivo.tar

SINOPSIS
      #include "s_mytarheader.h"
      int extrae_fichero(char * f_mytar, char * f_dat);

DESCRIPCIÓN
      La función extrae_fichero extrae un fichero (o directorio) del fichero 
      f_mytar. El fichero (o directorio) a extraer (nombre, datos,...) 
      se corresponde con la información indicada se corresponde con la información indicada
      en un "file system object" de f_mytar cuyo nombre coincide con el valor de f_dat.
	  
      - Si ese objeto corresponde a un elemento de tipo directorio, se debe crear el directorio indicado 
        en el campo name de c_header_gnu_tar.
      - Si lo que contine no es un directorio, se debe proceder como si fuera un 
        fichero regular:
        Si el nombre del elemento (contenido en el c_header_gnu_tar) es un fichero y 
        contine una ruta(directorio), si la ruta no existe, debe crearla. 
        Por ejemplo, si el nombre del archivo contenido en el c_header_gnu_tar 
        correspondiente a index, es dir1/fdatos.dat, y no existe el directorio dir/
        debe crear el directorio dir/ y sobre ese directorio crear el archivo 
        fdatos.dat
      - Si lo que contine no es un directorio ni un fichero, se comprobará y procesará como un
        enlace simbolico:
        Si el nombre del elemento (contenido en el c_header_gnu_tar) es un enlace simbolico y 
        contine una ruta(directorio), si la ruta no existe, debe crearla. 
        Por ejemplo, si el nombre del archivo contenido en el c_header_gnu_tar 
        correspondiente a index, es dir1/fdatos.dat, y no existe el directorio dir/
        debe crear el directorio dir/ y sobre ese directorio crear el archivo 
        fdatos.dat
      Si no es ninguno de los anteriores tipos descritos no hará nada.
   
VALOR DE RETORNO
       Si todo funciona correctamente, extrae_fichero devolverá cero. En caso contrario 
       no creará el fichero a extraer (cualquier caso) y (en caso de error de apertura unicamente)retornará los errores 
       indicados en el apartado de ERRORES.

ERRORES
       E_OPEN      (-1) 
           No se puede abrir f_mytar.

*/
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include "s_mytarheader.h"

// #define ERROR_OPEN_DAT_FILE (2)
// #define ERROR_OPEN_TAR_FILE (3)
// #define ERROR_GENERATE_TAR_FILE (4)
// #define ERROR_GENERATE_TAR_FILE2 (5)

// #define HEADER_OK (1)
// #define HEADER_ERR (2)

struct passwd *pws;
struct group *grp;
char *myDir;

char FileDataBlock[DATAFILE_BLOCK_SIZE];

//-----------------------------------------------------------------------------
// Return UserName (string) from uid (integer). See man 2 stat and man getpwuid
char *getUserName(uid_t uid)
{
    pws = getpwuid(uid);
    return pws->pw_name;
}

//------------------------------------------------------------------------------
// Return GroupName (string) from gid (integer). See man 2 stat and man getgrgid
char *getGroupName(gid_t gid)
{
    grp = getgrgid(gid);
    return grp->gr_name;
}

//----------------------------------------
// Return Mode type entry  (tar Mode type)
char mode_tar(mode_t Mode)
{
    if (S_ISREG(Mode))
        return '0';

    if (S_ISLNK(Mode))
        return '2';
    if (S_ISCHR(Mode))
        return '3';
    if (S_ISBLK(Mode))
        return '4';
    if (S_ISDIR(Mode))
        return '5';
    if (S_ISFIFO(Mode))
        return '6';
    if (S_ISSOCK(Mode))
        return '7';
    return '0';
}

unsigned long writeHeader(int fd_TarFile, struct c_header_gnu_tar *pheaderData)
{
    unsigned long NumWriteBytes;
    int n = 0;
    // write the data file (blocks of 512 bytes)
    NumWriteBytes = 0;
    printf("Datos Escritos en HEADER:"); // Traza
    n = write(fd_TarFile, pheaderData, sizeof(struct c_header_gnu_tar));
    NumWriteBytes = NumWriteBytes + sizeof(struct c_header_gnu_tar); // ojo!!!, no se escriben n
    printf("--%d -", n);                                             // Traza

    printf("\n Total :Escritos %ld \n", NumWriteBytes); // Traza
    return n;
}
// para evitar conflicto de tipos ¿?¿?¿?¿?
unsigned long WriteFileDataBlocks(int x, int y);
unsigned long WriteCompleteTarSize(unsigned long TarActualSize, int fd_TarFile);
unsigned long WriteEndTarArchive(int fd_TarFile);

unsigned long inserta_fichero(int f_mytar, unsigned long tamano, char *filename)
{
    int ret, f_dat, tam;
    unsigned long tamanoEscrito, n;
    long tamanio;
    long ficheros = 0;
    DIR *dir;
    struct dirent *directoryData;
    char entryNameAux[256];
    char linkmode[1] = "2";
    char linkvacio[100];
    struct c_header_gnu_tar my_tardat;
    tamanoEscrito = tamano;
    struct stat stattest;
    bzero(linkvacio, 100);
    int val = 0;

    // encontrar fin del fichero + mover apuntador
    if (tamano != 0)
    {
        while ((n = read(f_mytar, &my_tardat, sizeof(my_tardat))) > 0)
        {
            ficheros += sizeof(my_tardat);
            sscanf(my_tardat.size, "%011lo", &tamanio);
            if ((strcmp(my_tardat.magic, "ustar  ") != 0))
            {
                break;
            }
            else
            {
                val += 1;
                if ((strcmp(my_tardat.typeflag, linkmode) != 0) && (strcmp(my_tardat.typeflag, "5") != 0))
                {
                    ficheros += tamanio;
                    if (tamanio % 512 != 0)
                    {

                        tamanio += (512 - (tamanio % 512));
                        ficheros += (tamanio % 512);
                    }
                    lseek(f_mytar, tamanio, 1);
                }
            }
        }
        lseek(f_mytar, (long)(-sizeof(my_tardat)), 1);
        ficheros += (long)(-sizeof(my_tardat));
        if (val == 0)
        {
            fprintf(stderr, "Formato erroneo de: %i\n", my_tardat);
            return -3;
        }
    }
    if ((dir = opendir(filename)) != NULL)
    {
        BuilTarHeader(filename, &my_tardat);
        n = writeHeader(f_mytar, &my_tardat);
        ficheros += n;
        while ((directoryData = readdir(dir)) != NULL)
        {
            if ((strcmp(directoryData->d_name, "..") != 0) && (strcmp(directoryData->d_name, ".") != 0))
            {
                sprintf(entryNameAux, "%s/%s", filename, directoryData->d_name);
                BuilTarHeader(entryNameAux, &my_tardat);

                ficheros += n;
                printf("%s\n", entryNameAux);
                lstat(entryNameAux, &stattest);
                if (!(S_ISDIR(stattest.st_mode))) // mirar tipo con stat con en el struct dirent
                {
                    if (S_ISLNK(stattest.st_mode)) // comprobar si en enlace simbolico
                    {
                        n = writeHeader(f_mytar, &my_tardat);
                    }
                    else
                    {
                        n = writeHeader(f_mytar, &my_tardat);
                        if ((f_dat = open(entryNameAux, O_RDONLY)) == -1)
                        {
                            fprintf(stderr, "No se puede abrir el fichero de datos %s\n", entryNameAux);
                            return ERROR_OPEN_DAT_FILE;
                        }
                        n = WriteFileDataBlocks(f_dat, f_mytar);
                        ficheros += n;
                        close(f_dat);
                    }
                }
                else
                {
                    n = writeHeader(f_mytar, &my_tardat);
                    ficheros += n;
                }
            }
        }
    }
    else
    {
        // crear header + insertarlo
        BuilTarHeader(filename, &my_tardat);
        lstat(filename, &stattest);
        if (S_ISLNK(stattest.st_mode)) // comprobar si en enlace simbolico
        {
            n = writeHeader(f_mytar, &my_tardat);
            ficheros += n;
        }
        else
        {
            if ((f_dat = open(filename, O_RDONLY)) == -1)
            {
                fprintf(stderr, "No se puede abrir el fichero de datos %s\n", filename);
                return ERROR_OPEN_DAT_FILE;
            }
            n = writeHeader(f_mytar, &my_tardat);
            ficheros += n;
            // escribir archivo
            n = WriteFileDataBlocks(f_dat, f_mytar);

            ficheros += n;
            // escribir final
            close(f_dat);
        }
    }
    tam = WriteEndTarArchive(f_mytar);

    ficheros += (unsigned long)tam;
    if (tamanoEscrito != 0)
    {
        if (tamanoEscrito < ficheros)
        {
            tamanoEscrito = tamanoEscrito + (tamanoEscrito - ficheros);
        }
    }
    else
    {
        tamanoEscrito = ficheros;
    }

    // completar final

    tam = WriteCompleteTarSize(tamanoEscrito, f_mytar);
    printf("------BBBBBBBBBBBB --> %ld \n", tam);

    tamanoEscrito += (unsigned long)tam;
    // comprobar tamaÃ±o
    ret = VerifyCompleteTarSize((unsigned long)tamanoEscrito);
    if (ret < 0)
    {
        fprintf(stderr, "Error al generar el fichero tar %d. Tamanio erroneo %ld\n", f_mytar, tamanoEscrito);
        close(f_dat);
        close(f_mytar);
        return ERROR_GENERATE_TAR_FILE2;
    }

    printf("OK: Generado el fichero tar %d (size=%ld) con el contenido del archivo %d. \n", f_mytar, tamanoEscrito, f_dat);

    close(f_mytar);

    return 0;
}

// ------------------------------------------------------------------------
// (1.0) Build my_tardat structure with FileName stat info (See man 2 stat)
int BuilTarHeader(char *FileName, struct c_header_gnu_tar *pTarHeader)
{
    struct stat stat_file;

    ssize_t Symlink_Size;
    int n, i;
    char *pTarHeaderBytes;
    unsigned int Checksum;

    bzero(pTarHeader, sizeof(struct c_header_gnu_tar));

    if (lstat(FileName, &stat_file) == -1)
        return HEADER_ERR;

    strcpy(pTarHeader->name, FileName);
    sprintf(pTarHeader->mode, "%07o", stat_file.st_mode & 07777);                 // Only  the least significant 12 bits
    printf("st_mode del archivo %s %07o\n", FileName, stat_file.st_mode & 07777); // Only  the least significant 12 bits
    sprintf(pTarHeader->uid, "%07o", stat_file.st_uid);
    sprintf(pTarHeader->gid, "%07o", stat_file.st_gid);
    sprintf(pTarHeader->size, "%011lo", stat_file.st_size);
    sprintf(pTarHeader->mtime, "%011lo", stat_file.st_mtime);
    // checksum  the last     sprintf(pTarHeader->checksum, "%06o", Checksum);

    pTarHeader->typeflag[0] = mode_tar(stat_file.st_mode);

    //  linkname
    if (S_ISLNK(stat_file.st_mode))
        Symlink_Size = readlink(FileName, pTarHeader->linkname, 100);

    strncpy(pTarHeader->magic, "ustar ", 6); // "ustar" followed by a space (without null char)
    strcpy(pTarHeader->version, " ");        //   space character followed by a null char.
    strcpy(pTarHeader->uname, getUserName(stat_file.st_uid));
    strcpy(pTarHeader->gname, getGroupName(stat_file.st_gid));
    //  devmayor (not used)
    //  devminor (not used)
    sprintf(pTarHeader->atime, "%011lo", stat_file.st_atime);
    sprintf(pTarHeader->ctime, "%011lo", stat_file.st_ctime);
    //  offset (not used)
    //  longnames (not used)
    //  unused (not used)
    //  sparse (not used)
    //  isextended (not used)
    //  realsize (not used)
    //  pad (not used)

    // compute checksum (the last)
    memset(pTarHeader->checksum, ' ', 8); // Initialize to blanc spaces
    pTarHeaderBytes = (unsigned char *)pTarHeader;

    for (i = 0, Checksum = 0; i < sizeof(struct c_header_gnu_tar); i++)
        Checksum = Checksum + pTarHeaderBytes[i];

    sprintf(pTarHeader->checksum, "%06o", Checksum); // six octal digits followed by a null and a space character

    return HEADER_OK;
}

// ----------------------------------------------------------------
// (1.2) write the data file (blocks of 512 bytes)
unsigned long WriteFileDataBlocks(int fd_DataFile, int fd_TarFile)
{
    unsigned long NumWriteBytes;
    int Tam, n;

    // write the data file (blocks of 512 bytes)
    NumWriteBytes = 0;
    printf("Datos Escritos :"); // Traza
    bzero(FileDataBlock, sizeof(FileDataBlock));
    // memset(FileDataBlock, '\0', sizeof(FileDataBlock));  // Similar a bzero(...)
    while ((n = read(fd_DataFile, FileDataBlock, sizeof(FileDataBlock))) > 0)
    {
        write(fd_TarFile, FileDataBlock, sizeof(FileDataBlock));
        NumWriteBytes = NumWriteBytes + sizeof(FileDataBlock); // ojo!!!, no se escriben n
        printf("--%d -\n", n);
        printf("%c%c%c%c\n", FileDataBlock[0], FileDataBlock[1], FileDataBlock[2], FileDataBlock[3]); // Traza
        // NumWriteBytes = NumWriteBytes + DATAFILE_BLOCK_SIZE;
        bzero(FileDataBlock, sizeof(FileDataBlock));
    }

    printf("\n Total :Escritos %ld \n", NumWriteBytes); // Traza
    return NumWriteBytes;
}

// ----------------------------------------------------------------
// (2.1)Write end tar archive entry (2x512 bytes with zeros)
unsigned long WriteEndTarArchive(int fd_TarFile)
{
    unsigned long NumWriteBytes;
    int n;

    // write end tar archive entry (2x512 bytes with zeros)
    NumWriteBytes = 0;
    // To Do...
    bzero(FileDataBlock, sizeof(FileDataBlock));

    write(fd_TarFile, FileDataBlock, sizeof(FileDataBlock));
    NumWriteBytes += sizeof(FileDataBlock);

    write(fd_TarFile, FileDataBlock, sizeof(FileDataBlock));
    NumWriteBytes += sizeof(FileDataBlock);

    printf(" Escritos (End block)%d total %ld\n", n, NumWriteBytes); // Traza

    return NumWriteBytes;
}

// ----------------------------------------------------------------
// (2.2) complete Tar file to  multiple of 10KB size block
unsigned long WriteCompleteTarSize(unsigned long TarActualSize, int fd_TarFile)
{
    unsigned long NumWriteBytes;
    unsigned long Module, offset = 0;
    int n;
    char *buffer;

    NumWriteBytes = TarActualSize;
    unsigned long reference = 10240;
    // complete to  multiple of 10KB size blocks
    printf("TAR_FILE_BLOCK_SIZE=%ld  TarFileSize=%ld\n", TAR_FILE_BLOCK_SIZE, NumWriteBytes); // Traza
    unsigned long k = 10240;
    int mod_k = NumWriteBytes % k;
    int diff = 0;
    printf("\nBytes % k = %d \n", mod_k);
    printf("DATA BLOCK ::: %i \n", FileDataBlock);

    if (mod_k != 0)
    {

        diff = k - mod_k;

        printf("DIFF: %i \n", diff);

        bzero(FileDataBlock, sizeof(FileDataBlock));
        while (diff > DATAFILE_BLOCK_SIZE)
        {
            printf("DIFF: %ld \n", diff);
            write(fd_TarFile, FileDataBlock, sizeof(FileDataBlock));
            NumWriteBytes += sizeof(FileDataBlock);
            // k=k-sizeof(FileDataBlock);
            diff = diff - sizeof(FileDataBlock);
            offset += (unsigned long)sizeof(FileDataBlock);
        }
        printf("FIN --> %ld \n", NumWriteBytes);
        int size = diff * sizeof(char);
        buffer = (char *)malloc(size);
        bzero(buffer, size);
        NumWriteBytes += write(fd_TarFile, buffer, size);
        free(buffer);
        offset += (unsigned long)size;
        printf("FIN --> %ld \n", NumWriteBytes);
    }

    printf("OK: Generado el EndTarBlocks del archivo tar %ld bytes \n", NumWriteBytes); // Tr
    return offset;
}

// Verify Tar file zize to  multiple of 10KB size blocks
int VerifyCompleteTarSize(unsigned long TarActualSize)
{

    // Verify
    if ((TarActualSize % TAR_FILE_BLOCK_SIZE) != 0)
    {
        fprintf(stderr, "Error al generar el fichero tar. Tamanio erroneo %ld\n", TarActualSize);
        return ERROR_GENERATE_TAR_FILE2;
    }
    return 0;
}

int extrae_fichero(char *f_mytar, char *f_dat)
{
    struct c_header_gnu_tar pheaderData;
    int fd_DatFile, fd_TarFile, fileSize, permisos;
    long tamanio, tam;
    unsigned long n;
    char buff[512];
    int type;
    char *nombre = malloc(100);
    char *buffer = malloc(100);
    char *buffer2 = malloc(100);
    char name[100];
    int ruta = 1;
    tamanio = 0;
    printf("EXTRAER \n");

    if ((fd_TarFile = open(f_mytar, O_RDONLY)) == -1)
    {
        fprintf(stderr, "No se puede abrir el fichero tar %s\n", f_dat);
        return ERROR_OPEN_TAR_FILE;
    }

    while ((n = read(fd_TarFile, &pheaderData, sizeof(struct c_header_gnu_tar))) > 0)
    {
        if (strcmp(pheaderData.magic, "ustar  ") == 0)
        {
            // bytesJump = ((atoi(pheaderData.size) / 512) + 1) * 512;
            // printf("%s --> %i Bytes [ Datos de %i Bytes ] (%i) \n", pheaderData.name, atoi(pheaderData.size), bytesJump, atoi(pheaderData.mode));
            printf("salto=%lo\n", tamanio);
            sscanf(pheaderData.size, "%011lo", &tamanio);
            printf("salto=%lo\n", tamanio);
            // lseek(fd_TarFile, bytesJump, 1);
            printf("buscado=%s\n", f_dat);
            printf("esta=%s\n", pheaderData.name);
            printf("es igual=%i\n", strcmp(pheaderData.name, f_dat));
            // if the selected file name is equal to the argument file name, extract file
            if (strcmp(pheaderData.name, f_dat) == 0)
            {
                printf("%s\n", name);
                strcpy(name, pheaderData.name);
                printf("%s\n", name);
                nombre = pheaderData.name;
                permisos = strtol(pheaderData.mode, NULL, 8);
                printf("permisos=%d\n", permisos);
                printf("detectada ruta=%s\n", name);
                buffer2 = name;
                while (ruta)
                {
                    buffer = strtok(nombre, "/");
                    if (strcmp(buffer, buffer2) != 0)
                    {
                        printf("ruta a aniadir=%s\n", buffer);
                        printf("ruta a comparar=%s\n", buffer2);
                        if (opendir(buffer) == NULL)
                        {
                            if (mkdir(buffer, permisos) == -1)
                            {
                                fprintf(stderr, "No se puede crear el Directorio al extraer %s\n", f_dat);
                                return ERROR_OPEN_DAT_FILE;
                            }
                            printf("directorio %s creado\n", buffer);
                            chmod(buffer, 00755);
                            buffer2 = buffer;
                        }
                        printf("directorio %s creado\n", buffer);
                        buffer2 = buffer;
                        printf("ruta a comparar=%s\n", buffer2);
                    }
                    else
                    {
                        printf("finalizada creación de ruta\n");
                        ruta = 0;
                    }
                }
                printf("typeflag=%s\n", pheaderData.typeflag);
                bzero(buff, sizeof(buff));
                if (strcmp(pheaderData.typeflag, "0") == 0) // IS NORMAL FILE
                {
                    printf("en generar fichero\n");
                    sscanf(pheaderData.size, "%011lo", &tam);
                    printf("[[[[ FILE ]]]]\n");
                    printf("fichero=%s\n", f_dat);
                    // create file to extract
                    if ((fd_DatFile = open(f_dat, O_CREAT | O_RDWR | O_TRUNC)) == -1)
                    {
                        fprintf(stderr, "No se puede crear el fichero al extraer %s\n", f_dat);
                        return ERROR_OPEN_TAR_FILE;
                    }
                    while (tam > 0)
                    {
                        n = read(fd_TarFile, buff, sizeof(buff));
                        tam = tam - sizeof(buff);
                        n = write(fd_DatFile, buff, sizeof(buff));
                    }

                    // n = read(fd_TarFile, , atoi(pheaderData.size)); // read data to insert in the new file
                    // n = write(fd_DatFile, &buff, sizeof(buff));          // write data in the new file
                    close(fd_DatFile);
                    chmod(f_dat, permisos);
                }
                else if (strcmp(pheaderData.typeflag, "5") == 0) // IS DIRECTORY
                {
                    printf("[[[[ DIRECTORY ]]]]\n");

                    if (opendir(f_dat) == NULL)
                    {
                        printf("NO EXISTE EL DIRECTORIO, SE GENERA \n");
                        if (fd_DatFile = mkdir(f_dat, 0644) == -1)
                        {
                            fprintf(stderr, "No se puede crear el Directorio al extraer %s\n", f_dat);
                            return ERROR_OPEN_DAT_FILE;
                        }
                        chmod(f_dat, permisos);
                    }
                    else
                    {
                        printf("YA EXISTE EL DIRECTORIO \n");
                    }
                }
                else
                {
                    if (strstr(pheaderData.typeflag, "2") != NULL)
                    {
                        printf("[[[[ SYM LINK ]]]]\n");
                        printf("linkname=%s\n", pheaderData.linkname);
                        if (symlink(pheaderData.linkname, f_dat) == -1)
                        {
                            fprintf(stderr, "No se puede crear el enlace simbolico al extraer %s\n", f_dat);
                            return ERROR_OPEN_DAT_FILE;
                        }
                        chmod(f_dat, permisos);
                    }
                }

                // lseek(fd_TarFile, bytesJump - atoi(pheaderData.size), 1); // jump block
                close(fd_TarFile);
                return 0;
            }
            else
            {
                printf("es dir=%i\n", (strcmp(pheaderData.typeflag, "5")));
                printf("es sim link=%i\n", (strcmp(pheaderData.typeflag, "2")));
                if ((strcmp(pheaderData.typeflag, "5") != 0) && (strcmp(pheaderData.typeflag, "2") != 0))
                {
                    if (tamanio % 512 != 0)
                    {
                        tamanio += (512 - (tamanio % 512));
                    }
                    printf("salta\n");
                    lseek(fd_TarFile, tamanio, 1);
                }
            }
        }
    }
    free(nombre);
    free(buffer);
    free(buffer2);
    return 0;
}

int main(int argc, char *argv[])
{
    char FileName[256];
    char TarFileName[256];
    unsigned long TarFileSize, n, offset;
    ssize_t Symlink_Size;

    struct c_header_gnu_tar my_tardat;
    char FileDataBlock[DATAFILE_BLOCK_SIZE];

    int i, ret, Tam, tamano_cuerpo, leidos;

    int fd_DatFile, fd_TarFile;
    char *buffer;

    int numHeaders = 0;
    int puntero = 0;

    if (argc != 3 && argc != 4)
    {
        fprintf(stderr, "Uso: %s fichero  Tarfile.tar\n", argv[0]);
        fprintf(stderr, "Uso: %s -e fichero  Tarfile.tar\n", argv[0]);
        return 1;
    }
    if (argc == 4 && (strcmp(argv[1], "-e") != 0))
    {
        fprintf(stderr, "Uso: %s -e fichero  Tarfile.tar\n", argv[0]);
        return 1;
    }
    if (strcmp(argv[1], "-e") == 0)
    {
        return extrae_fichero(argv[3], argv[2]);
    }
    else if ((fd_TarFile = open(argv[2], O_RDWR, 0600)) == -1) // tar
    {
        if ((fd_TarFile = open(argv[2], O_RDWR | O_CREAT, 0600)) == -1)
        {
            fprintf(stderr, "No se puede abrir el fichero tar %s\n", argv[2]);
            close(fd_DatFile);
            return ERROR_OPEN_TAR_FILE;
        }
        inserta_fichero(fd_TarFile, 0, argv[1]);
        return 0;
    }
    else
    {
        struct stat sb;
        stat(argv[2], &sb);
        inserta_fichero(fd_TarFile, sb.st_size, argv[1]);
        return 0;
    }
}
