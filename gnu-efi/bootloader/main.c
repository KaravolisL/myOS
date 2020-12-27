#include <efi.h>
#include <efilib.h>
#include <elf.h>

typedef unsigned long long size_t;

EFI_FILE* loadFile(EFI_FILE* pDirectory, CHAR16* pPath, EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* pSystemTable)
{
    EFI_FILE* pLoadedFile;

    EFI_LOADED_IMAGE_PROTOCOL* pLoadedImage;
    pSystemTable->BootServices->HandleProtocol(imageHandle, &gEfiLoadedImageProtocolGuid, (void**)&pLoadedImage);

    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* pFileSystem;
    pSystemTable->BootServices->HandleProtocol(pLoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&pFileSystem);

    if (pDirectory == NULL)
    {
        pFileSystem->OpenVolume(pFileSystem, &pDirectory);
    }

    EFI_STATUS status = pDirectory->Open(pDirectory, &pLoadedFile, pPath, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);

    if (status != EFI_SUCCESS)
    {
        return NULL;
    }
    return pLoadedFile;
}

int memcmp(const void* pA, const void* pB, size_t numBytes)
{
    const unsigned char* pWorkingA = pA;
    const unsigned char* pWorkingB = pB;

    for (size_t i = 0; i < numBytes; i++)
    {
        if (pWorkingA[i] < pWorkingB[i])
        {
            return -1;
        }
        else if (pWorkingA[i] > pWorkingB[i])
        {
            return 1;
        }
    }
    return 0;
}

EFI_STATUS efi_main(EFI_HANDLE imageHandle, EFI_SYSTEM_TABLE* pSystemTable) {

    InitializeLib(imageHandle, pSystemTable);
    Print(L"Chop Wood, Carry Water\n\r");

    EFI_FILE* pKernel = loadFile(NULL, L"kernel.elf", imageHandle, pSystemTable);
    if (pKernel == NULL)
    {
        Print(L"Could not load kernel\n\r");
    }
    else
    {
        Print(L"Kernel loaded successfully\n\r");
    }

    Elf64_Ehdr header;
    {
        UINTN fileInfoSize;
        EFI_FILE_INFO* pFileInfo;
        pKernel->GetInfo(pKernel, &gEfiFileInfoGuid, &fileInfoSize, NULL);
        pSystemTable->BootServices->AllocatePool(EfiLoaderData, fileInfoSize, (void**)&pFileInfo);
        pKernel->GetInfo(pKernel, &gEfiFileInfoGuid, &fileInfoSize, (void**)&pFileInfo);

        UINTN size = sizeof(header);
        pKernel->Read(pKernel, &size, &header);
    }

    if (memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
        header.e_ident[EI_CLASS] != ELFCLASS64 ||
		header.e_ident[EI_DATA] != ELFDATA2LSB ||
		header.e_type != ET_EXEC ||
		header.e_machine != EM_X86_64 ||
		header.e_version != EV_CURRENT)
    {
        if (header.e_type != ET_EXEC)
        {
            Print(L"This is wrong\n\r");
        }
        if (header.e_machine != EM_X86_64)
        {
            Print(L"This is wrong2\n\r");
        }
        if (header.e_version != EV_CURRENT)
        {
            Print(L"This is wrong3\n\r");
        }
        Print(L"Kernel format is bad \n\r");
    }
    else
    {
        Print(L"Kernel header successfully verified\n\r");
    }

    Elf64_Phdr* pHeaders;
    {
        pKernel->SetPosition(pKernel, header.e_phoff);
        UINTN size = header.e_phnum * header.e_phentsize;
        pSystemTable->BootServices->AllocatePool(EfiLoaderData, size, (void**)&pHeaders);
        pKernel->Read(pKernel, &size, pHeaders);
    }

    for (
        Elf64_Phdr* pHeader = pHeaders;
        (char*)pHeader < (char*)pHeaders + header.e_phnum * header.e_phentsize;
        pHeader = (Elf64_Phdr*)((char*)pHeader + header.e_phentsize)
    )
    {
        switch (pHeader->p_type)
        {
            case PT_LOAD:
            {
                int pages = (pHeader->p_memsz + 0x1000 - 1) / 0x1000;
                Elf64_Addr segment = pHeader->p_paddr;
                pSystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

                pKernel->SetPosition(pKernel, pHeader->p_offset);
                UINTN size = pHeader->p_filesz;
                pKernel->Read(pKernel, &size, (void*)segment);
                break;
            }
        }
    }

    Print(L"Kernel Loaded\n\r");

    int (*pKernelStart)() = ((__attribute__((sysv_abi)) int (*)()) header.e_entry);

    Print(L"%d\n\r", pKernelStart());

	return EFI_SUCCESS; // Exit the UEFI application
}
