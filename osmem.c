// SPDX-License-Identifier: BSD-3-Clause

#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include "osmem.h"
#include "block_meta.h"

struct block_meta *start;
int ok = 2;

// functie care verifica daca un bloc trebuie impartit, apoi este creat noul bloc free
void split_block(struct block_meta *block, size_t size)
{
	if (block->size >= size + 32 && block->status == 0 && (block->size - size) > 32) {
		struct block_meta *new;

		new = (struct block_meta *)((char *)block + sizeof(struct block_meta) + size);
		new->size = block->size - size - sizeof(struct block_meta);
		new->status = 0;
		new->next = block->next;
		block->size = size;
		block->next = new;
	}
}

// functie care uneste 2 blocuri free adiacente
void coalesce(struct block_meta *block)
{
	while (block->next != NULL) {
		if (block->status == 0 && block->next->status == 0) {
			block->size = block->size + block->next->size + 32;
			block->next = block->next->next;
		} else {
			block = block->next;
		}
	}
}

// functie care sa unifice un bloc care trebuie sa fie expandat si surplusul
void unify(struct block_meta *block)
{
	while (block->next != NULL) {
		if (block->status == 0 && block->next->status == 0) {
			block->size = block->size + block->next->size;
			block->next = block->next->next;
		} else {
			block = block->next;
		}
	}
}

// functie care gaseste pozitia optima unde poate fi introdus un bloc
static struct block_meta *find(size_t size)
{
	struct block_meta *wanted = NULL;
	struct block_meta *block = start;
	size_t best = -1;

	while (block != NULL) {
		if (block->status == 0 && block->size >= size) {
			if (block->size - size < best) {
				best = block->size - size;
				wanted = block;
			}
		}
		block = block->next;
	}
	return wanted;
}

void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;
	// daca marimea e mai mare decat mmap_threshold sau daca e mai mare decat page la calloc
	if (size >= 1024 * 128 || ok == 1) {
		// alinierea
		if (size % 8 != 0)
			size = size / 8 * 8 + 8;
		struct block_meta *block;

		block = (struct block_meta *)mmap(NULL, size + 32, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		block->size = size;
		block->status = STATUS_ALLOC;
		block->status = STATUS_MAPPED;
		return (char *)block + sizeof(struct block_meta);
	}
		// initializare start
		if (start == NULL) {
			start = sbrk(128 * 1024);
			if (size % 8 != 0)
				size = size / 8  * 8 + 8;
			start->size = size;
			start->status = STATUS_ALLOC;
			start->next = NULL;
			return (char *)start + sizeof(struct block_meta);
		}
		// verific daca sunt blocuri care pot si unite
		coalesce(start);
		if (size % 8 != 0)
			size = size / 8 * 8 + 8;
		// gasesc pozitia unde inserez
		struct block_meta *block = find(size);
		// nu exista un loc bun
		if (block == NULL) {
			struct block_meta *aux = start;

			while (aux->next != NULL)
				aux = aux->next;
			// daca unltimul bloc e free aloc doar cat mai e nevoie in plus
			if (aux->status == 0) {
				block = (struct block_meta *)sbrk(size - aux->size);
				block->status = 0;
				block->next = NULL;
				block->size = size - aux->size;
				aux->next = block;
				unify(start);
				block = aux;
				block->status = STATUS_ALLOC;
			// altfel aloc un bloc nou
			} else {
				block = (struct block_meta *)sbrk(size + sizeof(struct block_meta));
				if (start->next == NULL)
					start->next = block;
				aux = start;
				while (aux->next != NULL)
					aux = aux->next;
				aux->next = block;
				block->size = size;
				block->status = STATUS_ALLOC;
				block->next = NULL;
			}
		} else {
			// verific daca blocul trebuie impartit apoi il marchez ca alocat
			if (size % 8 != 0)
				size = size / 8 * 8 + 8;
			if (block->size >= size)
				split_block(block, size);
			block->status = STATUS_ALLOC;
		}
		return (char *)block + sizeof(struct block_meta);
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (size == 0 || nmemb == 0)
		return NULL;
	// verific daca marimea e mai mare decat page ca sa stiu in ce caz sunt la malloc
	size_t page;

	page = getpagesize();
	if (nmemb * size + sizeof(struct block_meta) > page)
		ok = 1;
	// aloc memoria si o fac 0
	void *ptr;

	ptr = os_malloc(nmemb * size);
	for (size_t i = 0; i < nmemb * size; i++)
		*((char *)ptr + i) = 0;
	ok = 0;
	return ptr;
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		return os_malloc(size);
	// daca trebuie sa fac realloc cu un size 0, eliberez memoria si fac malloc de 0
	if (size == 0) {
		os_free(ptr);
		return os_malloc(size);
	}
	struct block_meta *block;

	block = (struct block_meta *)(ptr - sizeof(struct block_meta));
	// blocul care trebuie realocat nu exista
	if (block->status == 0)
		return NULL;
	// verific daca sunt blocuri care trebuie unite
	coalesce(start);
	// unesc blocul cu urmatorul daca e free, apoi fac split, asta daca impreuna au peste size
	if (block->next && (block->size + 2 * sizeof(struct block_meta) + block->next->size) >= size) {
		block->size = block->size + block->next->size - sizeof(struct block_meta);
		block->next = block->next->next;
		split_block(block, size);
		return ptr;
	}
	// vad daca trebuie sa fac split
	if (block->size >= size) {
		split_block(block, size);
		return ptr;
	}
	// aloc un nou bloc si copiez cat am nevoie in el
	char *new;

	new = os_malloc(size);
	memcpy(new, ptr, block->size);
	os_free(ptr);
	return new;
}

void os_free(void *ptr)
{
	if (ptr == NULL)
		return;
	struct block_meta *block;

	block = (struct block_meta *)(ptr - sizeof(struct block_meta));
	// e deja free
	if (block->status == 0)
		return;
	if (block->size % 8 != 0)
		block->size = block->size / 8 * 8 + 8;
	// folosesc munmap daca statusul e mapped
	if (block->status == 2) {
		munmap(ptr - sizeof(struct block_meta), block->size + sizeof(struct block_meta));
		return;
	}
	// marchez ca free si vad daca se uneste cu alte blocuri
	block->status = 0;
	coalesce(start);
}
