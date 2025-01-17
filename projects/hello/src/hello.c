#include <stdio.h>
#include <stdlib.h>
#include <crypt.h>
#include <sha256.h>

int main(void)
{
    char key[4] = "abc";
    char *res;
    int i;

    res = malloc(32*sizeof(char));
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, key, 3);
    sha256_final(&ctx, res);
    
    printf("Hello World!: \n");
    for ( i = 0; i < 32; i++)
    {
        printf("%x ", res[i]);
    }
    printf("\n");
    return 0;
}
