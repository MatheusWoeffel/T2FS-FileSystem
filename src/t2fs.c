
/**
*/
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/t2fs.h"
#include "../include/bitmap.h"
#include "../include/apidisk.h"
#include "../include/buffer_control.h"


/********************************************************************************/
/************************************ PRIVATE ***********************************/
/********************************************************************************/
bool initialized = false;	// Estado do sistema de arquivos: inicializado ou nao para correto funcionamento

char *currentPath;	// Caminho corrente do sistema de arquivos
DWORD currentDirIndexPointer; // Ponteiro de bloco de indice de diretorio associado ao caminho corrente
INDEX_BLOCK *rootDirIndex = NULL; // Bloco de indice da raiz

PART_INFO partInfo;	// Estrutura que armazenara enderecos e limites da particao
HANDLER openedFiles[MAX_FILE_OPEN];	// Estrutura armazenadora das informacoes dos arquivos abertos

void _printSuperblock(SUPERBLOCK superblock) {
	printf("\n\nSUPERBLOCK:\n");
	printf("ID: %.4s\n", superblock.id);
	printf("BlockSize: %d | 0x%x\n", superblock.blockSize, superblock.blockSize);
	printf("PartitionSize: %d | 0x%x\n", superblock.partitionSize, superblock.partitionSize);
	printf("NumberOfPointers: %d | 0x%x\n", superblock.numberOfPointers, superblock.numberOfPointers);
	printf("bitmapSectorsSize: %d | 0x%x\n", superblock.bitmapSectorsSize, superblock.bitmapSectorsSize);
	printf("dataBlockAreaSize: %d | 0x%x\n", superblock.dataBlockAreaSize, superblock.dataBlockAreaSize);
	printf("indexBlockAreaSize: %d | 0x%x\n", superblock.indexBlockAreaSize, superblock.indexBlockAreaSize);
}

void _printPartInfo() {
	printf("\n\nPARTINFO:\n");
	printf("BlockSize: %d | 0x%x\n", partInfo.blockSize, partInfo.blockSize);
	printf("dataBlocksStart: %d | 0x%x\n", partInfo.dataBlocksStart, partInfo.dataBlocksStart);
	printf("indexBlocksStart: %d | 0x%x\n", partInfo.indexBlocksStart, partInfo.indexBlocksStart);
	printf("firstSectorAddress: %d | 0x%x\n", partInfo.firstSectorAddress, partInfo.firstSectorAddress);
	printf("lastSectorAddress: %d | 0x%x\n", partInfo.lastSectorAddress, partInfo.lastSectorAddress);
	printf("numberOfPointers: %d | 0x%x\n", partInfo.numberOfPointers, partInfo.numberOfPointers);
}

DWORD getFreeIndexBlock() {
	DWORD offset = searchBitmap(BITMAP_INDEX, 1);
	setBitmap(BITMAP_INDEX, offset, 0);

	return (partInfo.indexBlocksStart + offset);
}

DWORD getFreeDataBlock() {
	DWORD offset = searchBitmap(BITMAP_DATA, 1);
	setBitmap(BITMAP_DATA, offset, 0);
	
	return (partInfo.dataBlocksStart + offset);
}

bool readPartInfoSectors() {
	BYTE buffer[sizeof(DWORD)];
	BYTE bufferSector[SECTOR_SIZE];

	if (read_sector(0, bufferSector) == 0) {
		partInfo.firstSectorAddress = bufferToDWORD(bufferSector, INIT_BYTE_PART_TABLE);
		partInfo.lastSectorAddress = bufferToDWORD(bufferSector, INIT_BYTE_PART_TABLE + sizeof(DWORD));
		return true;
	}

	return false;
}

bool readPartInfoBlocks() {
	BYTE sectorBuffer[SECTOR_SIZE];
	SUPERBLOCK superblock;

	if (read_sector(partInfo.firstSectorAddress, sectorBuffer) == 0) {
		memcpy(&superblock, sectorBuffer, sizeof(SUPERBLOCK));
		//TODO
		partInfo.dataBlocksStart = 0;
		partInfo.indexBlocksStart = 0;

		//Adição do número de ponteiros de indice
		partInfo.numberOfPointers = superblock.numberOfPointers;

	}
	
	return false;
}


void cleanDisk() {
	int i;
	int size = partInfo.lastSectorAddress - partInfo.firstSectorAddress + 1;
	BYTE cleanBuffer[256] = {0}; 
	for (i = 0; i < size; i++)
		write_sector(partInfo.firstSectorAddress + i, cleanBuffer);
	
}

SUPERBLOCK createSuperblock(int sectorsPerBlock) {
	SUPERBLOCK superblock;

	int size = partInfo.lastSectorAddress - partInfo.firstSectorAddress + 1; // Tamanho da particao em setores
	int totalBlocks =  (size - 1) / sectorsPerBlock; // Superbloco ocupa 1 setor
	int utilBlocks = (totalBlocks * sectorsPerBlock * SECTOR_SIZE * 8) / (sectorsPerBlock * SECTOR_SIZE * 8 + 1);
	int totalSectorsBitmap = ceil((float)utilBlocks / (float)(SECTOR_SIZE * 8));

	int indexBlocksSize = utilBlocks / 3; //Numero de blocos de indice
	int remainingBlocks = utilBlocks % 3; // Blocos que sobraram
	int dataBlocksSize = (utilBlocks - indexBlocksSize) + remainingBlocks; // Numero de blocos de dados

	char id[4] = {'T', '2', 'F', 'S'};
	memcpy(&superblock.id, &id, 4);
	superblock.bitmapSectorsSize = totalSectorsBitmap;
	superblock.dataBlockAreaSize = dataBlocksSize;
	superblock.indexBlockAreaSize = indexBlocksSize;
	superblock.blockSize = sectorsPerBlock;
	superblock.partitionSize = utilBlocks; // Em blocos
	superblock.numberOfPointers = sectorsPerBlock * SECTOR_SIZE /(int) sizeof(BLOCK_POINTER);

	return superblock;
}

int getBlockByPointer(BYTE *block,DWORD pointer, DWORD offset, int blockSize){
	int i,j;
	BYTE sectorBuffer[SECTOR_SIZE]; 
	DWORD initialSector = offset + (pointer * (DWORD)blockSize);

	for(i = 0; i < blockSize;i++){
		//Faz leitura e verifica se houve erro ao ler, se sim, aborta e retorna cod erro.
		if (read_sector(initialSector + i,sectorBuffer) != 0){
			printf("Erro ao ler setor ao preencher bloco");
			return SECTOR_ERROR;
		}
		//Copia setor para block
		for(j = 0; j < SECTOR_SIZE; j++){
			block[SECTOR_SIZE*i + j] = sectorBuffer[j];
		}

		
	}

}

int getIndexBlockByPointer(BYTE *indexBlock,DWORD pointer){
	return getBlockByPointer(indexBlock,pointer,partInfo.indexBlocksStart,partInfo.blockSize);
}

int getDataBlockByPointer(BYTE *dataBlock,DWORD pointer){
	return getBlockByPointer(dataBlock,pointer,partInfo.dataBlocksStart,partInfo.blockSize);
}

DIR_RECORD *getRecordByName(char *name) {
	//TODO
}

bool initPartInfo() {
	return (readPartInfoSectors() == true && readPartInfoBlocks() == true);
}

bool initRootDir() {
	//TODOOTODODTODOTODO MATHEUS
	rootDirIndex = getIndexBlockByPointer(0,0);
	
	if (rootDirIndex != NULL)
		return true;
	
	return false;
}

bool init() {
	if (initPartInfo() == true && initRootDir() == true) {
		int i;
		for (i = 0; i < MAX_FILE_OPEN; i++) {
			openedFiles[i].free = true;
			//...
		}

		currentPath = "/";
		//currentRecord = getRecordByName("."); // Nao sei pq o matheus removeu essa variavel. Dps eu pergunto

		initialized = true;
	}

	return false;
}

int findEmptyEntry(BYTE *indexBlock, BYTE *dataBlockBuffer, DWORD &blockPointer, int &initialByte);


DIR_RECORD createRecord(char *filename, int type) {
	DWORD indexBlockPointer = currentDirIndexPointer;
	int blockSizeInBytes = SECTOR_SIZE * partInfo.blockSize *sizeof(BYTE);
	int indexIterator, i, dirIterator;
	BLOCK_POINTER extractedPtr;
	BLOCK_POINTER ptrToIndexBlock;
	bool findInvalidEntry = false;

	//Buffers para fetch de bloco de indice e bloco de dados
	BYTE *indexBlockBuffer = malloc(blockSizeInBytes);
	BYTE *dataBlockBuffer = malloc(blockSizeInBytes);

	//Record a ser registrado 
	DIR_RECORD newRecord;
	newRecord.byteFileSize = 0;
	strcpy(newRecord.name,filename);
	newRecord.type = type;
	//TODO:Alocar bloco de indices e bloco de dados consistentes pro arquivo
	
	//Fetch do bloco de indices do diretorio corrente
	getIndexBlockByPointer(indexBlockBuffer,indexBlockPointer);
	int numberOfDirRecords = partInfo.blockSize * SECTOR_SIZE / sizeof(DIR_RECORD);
	
	while(!findEmptyEntry){
		for(indexIterator = 0; indexIterator < partInfo.numberOfPointers - 1; indexIterator++){
			extractedPtr = bufferToBLOCK_POINTER(indexBlockBuffer,indexIterator * sizeof(BLOCK_POINTER));

			if((char)extractedPtr.valid == INVALID_BLOCK_PTR){
				//TODO:aloca bloco de dados
				//aponta para bloco de dados
				//escreve newRecord no inicio
				//retorna
			}
			else{
				getDataBlockByPointer(dataBlockBuffer,extractedPtr.blockPointer);
				DIR_RECORD fetchedEntry;
				//Percorre as entradas de diretório no bloco
				for (dirIterator = 0; dirIterator < numberOfDirRecords; dirIterator++){
					fetchedEntry = bufferToDIR_RECORD(dataBlockBuffer, dirIterator * sizeof(DIR_RECORD));
					if(fetchedEntry.type == RECORD_INVALID){
						//TODO:Escreve modificações no bloco
						//Escreve no disco
						//retorna
					}
				}
			}
		}
		BLOCK_POINTER ptrToIndexBlock = bufferToBLOCK_POINTER(indexBlockBuffer,(partInfo.numberOfPointers - 1)* sizeof(BLOCK_POINTER));
		if(ptrToIndexBlock.valid == INVALID_BLOCK_PTR){
			//Aloca bloco de indices
			//Faz bloco antigo apontar para o novo
			DWORD newPtrIndexBlock = 0; //alocado acima
			getIndexBlockByPointer(indexBlockBuffer,newPtrIndexBlock);
		}
		else
			getIndexBlockByPointer(indexBlockBuffer,ptrToIndexBlock.blockPointer);
		
	}
	return newRecord;
}

bool isOpened(FILE2 handle) {
	return (handle >= 0 && openedFiles[handle].free == false);
}


int readFile(FILE2 handle, BYTE *buffer, int size){
	if(isOpened(handle))
		return ERROR;

	
}

/********************************************************************************/
/************************************ PUBLIC ************************************/
/********************************************************************************/

/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente o disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho 
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2 (int sectors_per_block) {
	BYTE buffer[SECTOR_SIZE] = {0};

	if (readPartInfoSectors() == true) {
		cleanDisk();
		
		// Escreve o superbloco 
		SUPERBLOCK superblock = createSuperblock(sectors_per_block);
		memcpy(buffer, &superblock, sizeof(SUPERBLOCK)); // so terá sentido se for usado mesmo endian na estrutura e no array
		_printSuperblock(superblock);

		if (write_sector(partInfo.firstSectorAddress, buffer) != 0)
			return ERROR;
		
		// Escreve o bitmap de indices e dados
		int i;
		for(i = 0; i < SECTOR_SIZE ; i++) { // Bitmap com todos bits em 1;
			buffer[i] = 0xFF;
		} 
		for (i = 0; i < superblock.bitmapSectorsSize; i++)
			if (write_sector(partInfo.firstSectorAddress + 1 + i, buffer) != 0)
				return ERROR;
		


		// Definindo ultimas informacoes sobre a particao
		partInfo.indexBlocksStart = partInfo.firstSectorAddress + superblock.bitmapSectorsSize + 1; // Inicio + bitmap + superbloco
		partInfo.dataBlocksStart = partInfo.indexBlocksStart + (superblock.indexBlockAreaSize * superblock.blockSize);
		partInfo.numberOfPointers = superblock.numberOfPointers;
		_printPartInfo();

		return SUCCESS;
	}
	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um 
		arquivo já existente, o mesmo terá seu conteúdo removido e 
		assumirá um tamanho de zero bytes.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	return createRecord(filename, RECORD_REGULAR);
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco. 
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle) {
	if (initialized == false && init() == false)
		return ERROR;
	
	if (handle >= MAX_FILE_OPEN || handle < 0)
		return ERROR;

	openedFiles[handle].free = true;
	return SUCCESS;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size) {
	if (initialized == false && init() == false)
		return ERROR;

	if (isOpened(handle) == true) 
		if (readFile(handle, buffer, size) == 0)
			return SUCCESS;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para truncar um arquivo. Remove do arquivo 
		todos os bytes a partir da posição atual do contador de posição
		(current pointer), inclusive, até o seu final.
-----------------------------------------------------------------------------*/
int truncate2 (FILE2 handle) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Altera o contador de posição (current pointer) do arquivo.
-----------------------------------------------------------------------------*/
int seek2 (FILE2 handle, DWORD offset) {
	if (initialized == false && init() == false)
		return ERROR;

	if(handle >= MAX_FILE_OPEN || handle < 0)
		return ERROR;

	if(openedFiles[handle].free == true)
		return ERROR;
	
	openedFiles[handle].pointer = offset;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo diretório.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um diretório do disco.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para alterar o CP (current path)
-----------------------------------------------------------------------------*/
int chdir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para obter o caminho do diretório corrente.
-----------------------------------------------------------------------------*/
int getcwd2 (char *pathname, int size) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink) com
		o nome dado por linkname (relativo ou absoluto) para um 
		arquivo ou diretório fornecido por filename.
-----------------------------------------------------------------------------*/
int ln2 (char *linkname, char *filename) {
	if (initialized == false && init() == false)
		return ERROR;

	return ERROR;
}


