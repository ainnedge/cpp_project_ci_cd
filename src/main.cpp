#include <cpp_project_ci_cd/cpp_project_ci_cd.h>

#include <iostream>

int main(int argc, char **argv){
	std::cout << cppProjectCiCd::sayHello() << std::endl;

	pcidrif_open();

	char data[1024];
	pcidr_read(2, 0x12000000, 1024, data);
	for (int i = 0; i < 1024; i++)
		printf("%hhd\n", data[i]);

	pcidrif_close();
	
	return 0;
}
