/*******************************************************************
 *          CopyRight(C) 2022-2026  Fibocom Wireless Inc
 *******************************************************************
 * FileName : main.c
 * Author   : Frank.zhou
 * Date     : 2022.05.25
 * Used     : Generate the h files from files
 *******************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>


int main(int argc, char **argv)
{
    int ret = -1, i, write_len = 0, file_size = 0;
    char default_name[512] = {0}, newname[512] = {0},H_Name[512] = {0};
    char writebuf[512] = {0};
    uint8_t *readbuf = NULL;
    FILE *fp_read = NULL, *fp_write = NULL;
    char *tmp = NULL;
    char *filename = argv[1];

    if(argc != 2 || argv[1] == NULL) {
        printf("The input param is error.\n");
        return -1;
    }

    fp_read = fopen(filename, "rb");
    if (fp_read == NULL){
        printf("open '%s' failed! '%s'\n", filename, strerror(errno));
        goto END;
    }

    fseek(fp_read, 0, SEEK_END);
    file_size = ftell(fp_read);
    fseek(fp_read, 0, SEEK_SET);
    printf("filename: %s, file_size:%d\n", filename, file_size);

    readbuf = malloc(file_size);
    if (readbuf == NULL) {
        printf("malloc readbuf failed.\n");
        fclose(fp_read);
        goto END;
    }

    if (fread(readbuf, 1, file_size, fp_read) != file_size) {
        printf("read '%s' failed! '%s'\n", filename, strerror(errno));
        goto END;
    }
    
    strcpy(H_Name, filename);
    tmp = strrchr(H_Name, '.');
    if (tmp) {
        *tmp = '_';
    }

    snprintf(newname, sizeof(newname), "%s.h", H_Name);
    fp_write = fopen(newname, "wb");
    if (fp_write == NULL){
        printf("open '%s' failed! '%s'\n", newname, strerror(errno));
        goto END;
    }

    tmp = strrchr(H_Name, '/');
    if (tmp == NULL) {
        tmp = H_Name;
    } else {
        tmp++;
    }
    strcpy(default_name, tmp);
    strcpy(H_Name, default_name);

    for(i=0; H_Name[i] ; i++) {
        H_Name[i] = toupper(H_Name[i]);
    }

    write_len = sprintf(writebuf, "#ifndef __%s_H__\n", H_Name);
    if (fwrite(writebuf, 1, write_len, fp_write) != write_len) {
        printf("fwrite '%s' failed! '%s'\n", newname, strerror(errno));
        goto END;
    }

    write_len = sprintf(writebuf, "#define __%s_H__\n\n", H_Name);
    if (fwrite(writebuf, 1, write_len, fp_write) != write_len) {
        printf("fwrite '%s' failed! '%s'\n", newname, strerror(errno));
        goto END;
    }

    write_len = sprintf(writebuf, "char %s_buf[] = {\n    ", default_name);
    if (fwrite(writebuf, 1, write_len, fp_write) != write_len) {
        printf("fwrite '%s' failed! '%s'\n", newname, strerror(errno));
        goto END;
    }

    for(i=0; i<file_size; i++) {
        write_len = sprintf(writebuf, "0x%02X, ", readbuf[i]);
        if (((i+1)%16) == 0) {
            write_len += sprintf(writebuf + write_len, "\n    ");
        }

        if (fwrite(writebuf, 1, write_len, fp_write) != write_len) {
            printf("fwrite '%s' failed! '%s'\n", newname, strerror(errno));
            goto END;
        }
    }
    
    write_len = sprintf(writebuf, "\n};\n\n#endif\n");
    if (fwrite(writebuf, 1, write_len, fp_write) != write_len) {
        printf("fwrite '%s' failed! '%s'\n", newname, strerror(errno));
        goto END;
    }

    printf("generated file: %s\n", newname);
    ret=0;
END:
    
    if (fp_write) {
        fclose(fp_write);
    }
    
    if (fp_read) {
        fclose(fp_read);
    }
    
    if (readbuf) {
        free(readbuf);
    }

    return ret;
    

    return 0;
}
