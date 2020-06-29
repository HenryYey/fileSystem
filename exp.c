#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<malloc.h>
#include"disk.h"
#include"operate.h"
#include<time.h>

/* 操作部分 */
 
void start()
{
    //创建空间
    systemStartAddr = (char*)malloc(SYSTEMSIZE * sizeof(char));
    // 初始化盘块的位示图
    for(int i=0; i<BLOCKCOUNT; i++)
        systemStartAddr[i] = '0';
    int size = BLOCKCOUNT * sizeof(char) / BLOCKSIZE;//位示图占用盘块数
    for(int i=0; i<size; i++) // 从零开始分配
        systemStartAddr[i] = '1'; //1表示盘块已被使用

    // 分配空间给根目录
    int startBlock = getBlock(1);
    rootdir = (struct dir*)getBlockAddr(startBlock);
    rootdir->total = 0;
    currentDir = rootdir;
}
//退出系统
void exitSystem()
{
    free(systemStartAddr);
}
// 分配磁盘块
int getBlock(int blockSize)
{
    int startBlock = 0;
    int sum = 0;
    for(int i = 0; i<BLOCKCOUNT; i++)
    {
        if(systemStartAddr[i] == '0') // 0为可用盘块
        {
            if(sum == 0)
        startBlock = i;
            sum++;
            if(sum == blockSize)
            {
        for(int j=startBlock; j<startBlock+blockSize; j++)
            systemStartAddr[j] = '1'; // 1表示分配盘块
        return startBlock;
            }
        } else sum = 0;
    }
    printf("get block fail, because memory is full or hasn't contiguous memory\n");
    return -1;
}
// 获得盘块的物理地址
char* getBlockAddr(int blockNum)
{
    return systemStartAddr + blockNum * BLOCKSIZE; //单位为byte
}

int getAddr(char* addr)
{
    return (addr - systemStartAddr)/BLOCKSIZE;
}

// 释放盘块
int releaseBlock(int blockNum, int blockSize)
{
    int endBlock = blockNum + blockSize;
    for(int i=blockNum; i<endBlock; i++)
        systemStartAddr[i] = '0';
    return 0;
}

// 创建文件, touch fileName size auth
int createFile(char fileName[], int fileSize, int auth)
{
    // 获得1kb给FCB的空间
    int fcbBlock = getBlock(1);
    if(fcbBlock == -1) {
        printf("get block fail");
        return -1;
    }
    // 获取文件数据空间
    int FileBlock = getBlock(fileSize);
    if(FileBlock == -1) {
        printf("get block fail");
        return -1;
    }
    // 创建FCB
    if(creatFCB(fcbBlock, FileBlock, fileSize, auth) == -1) {
        printf("create FCB fail");
        return -1;
    }
    //添加到当前目录

    addToDir(currentDir, fileName, 0, fcbBlock);
 
    return 0;
}
// 创建FCB
int creatFCB(int fcbBlockNum, int fileBlockNum, int fileSize, int auth)
{  
    struct FCB* fcb = (struct FCB*) getBlockAddr(fcbBlockNum);
    fcb->fileSize = fileSize;
    fcb->blockNum = fileBlockNum; // 文件数据起始位置
    fcb->dataSize = 0; // 文件已写入数据长度
    fcb->readptr = 0; // 文件读指针
    fcb->protect = auth; // 权限标志, 0为不可读不可写，1为只读，2为可读可写
    return 0;
}

// 创建目录 mkdir
int mkdir(char name[])
{
    //为目录表分配空间
    int fcbBlock = getBlock(1);
    if(fcbBlock == -1) {
        printf("get block fail");
        return -1;
    }
    // 添加到当前目录下
    addToDir(currentDir, name, 1, fcbBlock);

    struct dir* newDir = (struct dir*)getBlockAddr(fcbBlock);
    newDir->total = 0;
    char parent[] = "../";
    // 为该目录创建父目录，这样就能实现cd ../上一级跳转
    addToDir(newDir, parent, 0, getAddr((char*)currentDir));
    return 0;
}

// 添加到目录
int addToDir(struct dir* dir, char fileName[], int type, int fcbBlock)
{
    int total = dir->total;
    struct dirFile* newFile = &dir->items[total];
    dir->total++;

    strcpy(newFile->fileName, fileName);
    newFile->startBlock = fcbBlock;
    newFile->type = type; // 0是文件 1是目录
    time_t nowtime;
    nowtime = time(NULL); //获取系统当前时间
    newFile->time = ctime(&nowtime);

    return 0;
}

// 切换目录 cd dirName
int changeDir(char dirName[])
{
    // 这里包含了 cd ../
    int index = findFile(dirName);
    if(index == -1)
    {
        printf("Not found\n");
        return -1;
    }
    if(currentDir->items[index].type == 0)
    {
        printf("Not a dir\n");
        return -1;
    }

    int block = currentDir->items[index].startBlock;
    currentDir = (struct dir*)getBlockAddr(block);

    return 0;
}

// 默认在当前目录下查找文件，返回索引
int findFile(char name[])
{
    int total = currentDir->total;
    for(int i = 0; i < total; i++) // 获取文件下标
       if(strcmp(name, currentDir->items[i].fileName) == 0)
            return i;
    return -1; // not found
}
// 重命名 mv oldName newName
int renameFile(char oldName[], char newName[])
{
    int index = findFile(oldName);
    if(index == -1)
    {
        printf("Not found\n");
        return -1;
    }
    struct dirFile* item = &currentDir->items[index];
    int fcbBlock = item->startBlock;

    // 重命名
    strcpy(item->fileName, newName);
    
    return 0;
}
// 删除文件或目录 rm name
int renameItem(char name[])
{
    int index = findFile(name);
    if(index == -1)
    {
        printf("Not found\n");
        return -1;
    }
    struct dirFile item = rootdir->items[index];
    int fcbBlock = item.startBlock;
    if (item.type == 0) {
        // 删除文件
        doDelete(index);
    } else {
        // 删除目录
        removeDir(name);
    }
    return 0;
}

// 删除目录 相当于rm -rf
int removeDir(char dirName[])
{
    int index = findFile(dirName);
    if(index == -1)
    {
        printf("Not found\n");
        return -1;
    }

    struct dirFile *item = &currentDir->items[index];
    int fcbBlock = item->startBlock;
    struct dir* temp = (struct dir*)getBlockAddr(fcbBlock);
    // 递归删掉该目录下的文件
    for(int i=1; i<temp->total; i++)
    {
        renameItem(temp->items[i].fileName);
    }
    //释放目录表空间
    releaseBlock(fcbBlock, 1);

    deleteInParentDir(index);
    return 0;
}
// 实际删除文件操作
int doDelete(int index)
{
    struct dirFile *item = &currentDir->items[index];
    int fcbBlock = item->startBlock;
    // 从当前目录下删除
    deleteInParentDir(index);
    // 释放内存
    releaseMemory(fcbBlock);
    return 0;
}
// 在当前目录中移除目录项
int deleteInParentDir(int unitIndex)
{
    for(int i=unitIndex; i<currentDir->total-1; i++)
    {
        currentDir->items[i] = currentDir->items[i+1];
    }
    currentDir->total--;
    return 0;
}

// 释放内存
int releaseMemory(int fcbBlock)
{
    struct FCB* fcb = (struct FCB*)getBlockAddr(fcbBlock);
    releaseBlock(fcb->blockNum, fcb->fileSize);
    releaseBlock(fcbBlock, 1);
    return 0;
}

// 读文件 read fileName length
int readFile(char fileName[], int length)
{
    int index = findFile(fileName);
    if(index == -1)
    {
        printf("Not found\n");
        return -1;
    }
    int fcbBlock = currentDir->items[index].startBlock;
    struct FCB* fcb = (struct FCB*)getBlockAddr(fcbBlock);
    if (fcb->protect < 1) {
        printf("Permission denied \n"); // 没有读权限
        return -1;
    }
    int dataSize = fcb->dataSize;
    char* data = (char*)getBlockAddr(fcb->blockNum);
    for(int i=0; i<length && fcb->readptr < dataSize; i++, fcb->readptr++)
    {
        printf("\033[31m%c\033[0m", *(data + fcb->readptr));
    }
    printf("\n");
    return 0;
}

// 写文件，从末尾写入 touch fileName content
int writeFile(char fileName[], char content[])
{
    int index = findFile(fileName);
    if(index == -1)
    {
        printf("Not found\n");
        return -1;
    }
    int FCBBlock = rootdir->items[index].startBlock;
    struct FCB* fcb = (struct FCB*)getBlockAddr(FCBBlock);
    if (fcb->protect < 2) {
        printf("Permission denied \n"); // 没有读写权限
        return -1;
    }
    int fileSize = fcb->fileSize * BLOCKSIZE;
    char* data = (char*)getBlockAddr(fcb->blockNum);
    for(int i=0; i<strlen(content) && fcb->dataSize<fileSize; i++, fcb->dataSize++)
    {
        *(data+fcb->dataSize) = content[i];
    }
    return 0;
}
// 展示当前目录 ls
void ls()
{
    int total = currentDir->total;
    for(int i=0; i<total; i++)
    {
        struct dirFile item = currentDir->items[i];
        if (strcmp(item.fileName, "../") == 0) continue; // 忽略../
        else if (item.type == 1) printf("\033[37m%s\t\033[0m", item.fileName); // 如果是文件夹，输出蓝色
        else printf("\033[32m%s\t\033[0m", item.fileName);
    }
    printf("\n");
}
// 展示提示信息1
void help() {
    printf("*********************2017132090 System*******************************\n\n");
    printf("命令名\t\t命令参数\t\t命令说明\n\n");
    printf("ls\t\t无\t\t\t显示文件目录结构\n");
    printf("touch\t\t文件名 大小 权限\t在当前目录下创建指定大小文件，并给用户权限\n");
    printf("mkdir\t\t目录名\t\t\t在当前目录下创建文件夹\n");
    printf("rm\t\t文件名/目录\t\t在当前目录下删除指定文件/目录\n");
    printf("mv\t\t旧文件名 新文件名\t重命名\n");
    printf("write\t\t文件名 数据\t\t从末尾开始写数据到该文件\n");
    printf("read\t\t文件名 长度\t\t读取该文件\n");
    printf("cd\t\t目录名\t\t\t切换到该目录\n");
    printf("help\t\t无\t\t\t提示信息\n");
    printf("exit\t\t无\t\t\t退出系统\n\n");
    printf("*********************************************************************\n\n");
}


void do_main()
{
    // 循环读取指令
    int flag = 1;
    while(flag)
    {
        char cmd[100];
        char command[100];
        // 3个变量存可选参数 这个系统最多就3个
        char option1[100];
        char option2[100];
        char option3[100];
        printf("2017132090@hengye:~>");
        char space[] = " "; //定义空格变量
        gets(cmd); // 直至换行
        // 分割字符串
        char *p = strtok(cmd, space);
        strcpy(command, p);
        p = strtok(NULL,space);
        if(p) {
            strcpy(option1, p);
            p = strtok(NULL,space); 
        }
        if(p) {
            strcpy(option2, p);
            p = strtok(NULL,space); 
        }
        if(p) {
            strcpy(option3, p);
        }
        // 根据指令和参数执行对应操作
        if (strcmp(command, "ls") == 0) {
            ls();
        } else if (strcmp(command, "cd") == 0) {
            // cd dirName
            changeDir(option1);
        } else if (strcmp(command, "help") == 0) {
            help();
        } else if (strcmp(command, "touch") == 0) {
            // touch fileName fileSize auth
            // 如果没有输入权限，则默认给读写权限
            int auth = 2; // 0为不可读不可写，1为只读，2为可写可读
            int len = 100;
            if (strlen(option2) != 0) len = atoi(option2); // 转化为整形
            if (strlen(option3) != 0) auth = atoi(option3); // 转化为整形
            createFile(option1, len, auth);
        } else if (strcmp(command, "read") == 0) {
            // read fileName length
            int len = 100;
            if (strlen(option2) != 0) len = atoi(option2); // 转化为整形
            readFile(option1, len);
        } else if (strcmp(command, "write") == 0) {
            // write fileName content
            writeFile(option1, option2);
        } else if (strcmp(command, "mv") == 0) {
            // mv oldName newName
            renameFile(option1, option2);
        } else if (strcmp(command, "mkdir") == 0) {
            // mkdir name
            mkdir(option1);
        } else if (strcmp(command, "rm") == 0) {
            // rm name
            renameItem(option1);
        } else if (strcmp(command, "exit") == 0) {
            // exit
            exitSystem();
            flag = 0; // 退出循环
        } else {
            printf("command not found \n");
        }
    }
}

int main()
{
    start();
    do_main();
    return 0;
}