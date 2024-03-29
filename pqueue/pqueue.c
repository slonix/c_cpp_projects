/*
 * author: Konrad Sloniewski
 * date:   23 Nov 2012
 */

#include <minix/drivers.h>
#include <minix/driver.h>
#include <stdio.h>
#include <stdlib.h>
#include <minix/ds.h>
#include <math.h>
#include <string.h>

#define min( a, b ) ( ((a) < (b)) ? (a) : (b) )

/**************************************************************************
                                 TYPEDEFS
**************************************************************************/

typedef struct pqueue_entry {
    u32_t key, value;
} pqueue_entry_t;

typedef struct pqueue_element {
    pqueue_entry_t elem;
    struct pqueue_element * next;
} pqueue;

typedef struct process_element {
	int proc_nr, elem_read, bytes;
	iovec_t *iov;
	struct process_element * next;
} process;

/**************************************************************************
                           FUNCTION PROTOTYPES
**************************************************************************/

FORWARD _PROTOTYPE( char * pqueue_name, (void) );
FORWARD _PROTOTYPE( int pqueue_open, (struct driver *d, message *m) );
FORWARD _PROTOTYPE( int pqueue_close, (struct driver *d, message *m) );
FORWARD _PROTOTYPE( struct device * pqueue_prepare, (int device) );
FORWARD _PROTOTYPE( int pqueue_transfer, (int procnr, int opcode,
                    u64_t position, iovec_t *iov, unsigned nr_req) );
FORWARD _PROTOTYPE( void pqueue_geometry, (struct partition *entry) );
FORWARD _PROTOTYPE( void sef_local_startup, (void) );
FORWARD _PROTOTYPE( int sef_cb_init, (int type, sef_init_info_t *info) );
FORWARD _PROTOTYPE( int sef_cb_lu_state_save, (int) );
FORWARD _PROTOTYPE( int lu_state_restore, (void) );
FORWARD _PROTOTYPE( int pqueue_other, (struct driver *d, message *m) );

/**************************************************************************
                               ENTRY POINTS
**************************************************************************/

PRIVATE struct driver pqueue_tab =
{
    pqueue_name,
    pqueue_open,
    pqueue_close,
    nop_ioctl,
    pqueue_prepare,
    pqueue_transfer,
    nop_cleanup,
    pqueue_geometry,
    nop_alarm,
    nop_cancel,
    nop_select,
    pqueue_other,
    do_nop,
};

/**************************************************************************
                            PRIVATE VARIABLES
**************************************************************************/

PRIVATE struct device pqueue_device;
PRIVATE int open_counter;
PRIVATE int elem_counter;
PRIVATE int proc_counter;
PRIVATE int status_counter;
PRIVATE int revive_pending;
PRIVATE int vfs_id;
PRIVATE pqueue * dummy_pqueue;
PRIVATE process * dummy_process;

/**************************************************************************
                        DRIVER CONTROL FUNCTIONS
**************************************************************************/

PRIVATE char * pqueue_name(void)
{
    return "pqueue";
}

PRIVATE int pqueue_open(d, m)
    struct driver *d;
    message *m;
{
    vfs_id = m->m_source;
    return OK;
}

PRIVATE int pqueue_close(d, m)
    struct driver *d;
    message *m;
{
    return OK;
}

PRIVATE struct device * pqueue_prepare(dev)
    int dev;
{
    pqueue_device.dv_base.lo = 0;
    pqueue_device.dv_base.hi = 0;
    pqueue_device.dv_size.lo = 0;
    pqueue_device.dv_size.hi = 0;
    return &pqueue_device;
}

PRIVATE void pqueue_geometry(entry)
    struct partition *entry;
{
    entry->cylinders = 0;
    entry->heads     = 0;
    entry->sectors   = 0;
}

/**************************************************************************
                           PQUEUE_ADD_ELEMENT
**************************************************************************/

PRIVATE int higher(int k1, int k2, int v1, int v2) {
	int bool = 0;
	if ((k1 > k2) || ((k1 == k2) && (v1 > v2))) bool = 1;
	return bool;
}

PRIVATE void pqueue_add_element(char * element)
{
	char * all;
	char * key;
	char * value;
	pqueue * iterator = dummy_pqueue;
	pqueue * tmp_pqueue = (pqueue *) malloc(sizeof(pqueue));

	struct pqueue_entry entry = *((struct pqueue_entry *) element);
	tmp_pqueue->elem = entry;

	tmp_pqueue->next = NULL;

	/* Adding element */
	while ((iterator->next != NULL) &&
		higher(tmp_pqueue->elem.key, iterator->next->elem.key,
		tmp_pqueue->elem.value, iterator->next->elem.value)) {
		iterator = iterator->next;
	}
	tmp_pqueue->next = iterator->next;
	iterator->next = tmp_pqueue;
	elem_counter++;
}

/**************************************************************************
                            PQUEUE_GET_ELEMENT
**************************************************************************/

PRIVATE char * pqueue_get_element()
{
	pqueue * tmp_pqueue;
	pqueue * remove;
	char * return_element = malloc(sizeof(char) * (sizeof(pqueue_entry_t) + 1));
	char * outcome = malloc(sizeof(char) * sizeof(pqueue_entry_t));
	int i = 0;

	remove = dummy_pqueue;
	tmp_pqueue = dummy_pqueue->next;

	if (tmp_pqueue == NULL) {
		return "";
	}

	elem_counter--;
	dummy_pqueue = dummy_pqueue->next;

	free(remove);

	/* Changing pqueue_entry_t into char * */

	memcpy(return_element, &(tmp_pqueue->elem), sizeof(pqueue_entry_t));
	return_element[sizeof(pqueue_entry_t) + 1] = '0';

	for (i = 0; i <= 7; i++) {
		outcome[i] = return_element[i];
	}
	free(return_element);

	return outcome;
}

/**************************************************************************
                        PQUEUE_GET_SOME_ELEMENTS
**************************************************************************/

PRIVATE char * pqueue_get_some_elements(int elem_read, int bytes)
{
	char * read_elements1;
	char * read_elements2;
	char * element;
	int i = 0;
	int position = 0;
	read_elements1 = malloc(sizeof(char) * bytes);

	while ((elem_read > 0) && (elem_counter > 0)) {
		element = pqueue_get_element();
		while ((i < 8) && (position < bytes)) {
			read_elements1[position] = element[i];
			i++;
			position++;
		}
		free(element);
		i = 0;
		elem_read--;
	}

	if (position == bytes) {
		return read_elements1;
	} else {
		read_elements2 = malloc(sizeof(char) * (position + 1));
		i = 0;

		while (i <= position) {
			read_elements2[i] = read_elements1[i];
			i++;
		}

		free(read_elements1);
		return read_elements2;
	}
}

/**************************************************************************
                             PRINT_ELEMENTS
**************************************************************************/

/*PRIVATE void print_elements()
{
	char * all;
	pqueue * iterator;
	all = malloc(sizeof(char) * (sizeof(struct pqueue_entry) + 1));
	iterator = dummy_pqueue->next;
	printf("print_elements: Elementy w kolejce to:");

	while (iterator != NULL) {
		memcpy(all, &(iterator->elem), sizeof(struct pqueue_entry));
		all[sizeof(struct pqueue_entry) + 1] = '0';
		printf(" %s", all);
		iterator = iterator->next;
	}

	free(all);
	printf("\n");
}*/

/**************************************************************************
                             PRINT_PROCESS
**************************************************************************/

/*PRIVATE void print_process()
{
	process * iterator;
	iterator = dummy_process;

	while (iterator->next != NULL) {
		iterator = iterator->next;
	}
}*/


/**************************************************************************
                              PQUEUE_OTHER
**************************************************************************/

/*PRIVATE void print_process()
{
	process * iterator;
	iterator = dummy_process;

	while (iterator->next != NULL) {
		iterator = iterator->next;
	}
}*/

/**************************************************************************
                         PQUEUE_ADD_WAITING_PROC
**************************************************************************/

PRIVATE void pqueue_add_waiting_proc(proc_nr, iov)
	int proc_nr;
	iovec_t *iov;
{
	process * iterator = dummy_process;
	process * tmp_process = (process *) malloc(sizeof(process));
	tmp_process->proc_nr = proc_nr;
	tmp_process->iov = iov;

	/* Adding process */
	while (iterator->next != NULL) {
		iterator = iterator->next;
	}

	tmp_process->next = iterator->next;
	iterator->next = tmp_process;
	proc_counter++;
}

/**************************************************************************
                              GET_IOV
**************************************************************************/

PRIVATE iovec_t* get_iov(void) {
	process * tmp_process = dummy_process;
	iovec_t* iov = dummy_process->next->iov;

	dummy_process = dummy_process->next;
	free(tmp_process);
	return iov;
}

/**************************************************************************
                              PQUEUE_OTHER
**************************************************************************/

PRIVATE int pqueue_other(dp, m_ptr)
    struct driver * dp;
    message * m_ptr;
{
    int bytes, elem_read, ret, send_bytes, proc_nr;
    char * element_out;
    iovec_t* iov;

    proc_nr = 1;
    switch(m_ptr -> m_type) {
        case DEV_STATUS: {
            if (dummy_process->next == NULL || !elem_counter) {
                m_ptr->m_type = DEV_NO_STATUS;
                send(m_ptr->m_source, m_ptr);
            } else {
                iov = get_iov();
                bytes = iov->iov_size;

                elem_read = min(elem_counter, (int) ceil(iov->iov_size / 8.0));
                bytes = iov->iov_size;

                if (elem_read * 8 < bytes) {
                    send_bytes = elem_read * 8;
                } else {
                    send_bytes = bytes;
                }

                element_out = pqueue_get_some_elements(elem_read, send_bytes);
                ret = sys_safecopyto(proc_nr, iov->iov_addr, 0,
                    (vir_bytes) element_out, send_bytes, D);

                free(element_out);

                m_ptr->REP_STATUS = iov->iov_size;
                m_ptr->REP_IO_GRANT = iov->iov_addr;
                m_ptr->m_type = DEV_REVIVE;
                m_ptr->REP_ENDPT = proc_nr;
                send(m_ptr->m_source, m_ptr);
            }
            break;
        }

        default : {
	    return EINVAL;
	}
    }
    return EDONTREPLY;
}

/**************************************************************************
                             PQUEUE_TRANSFER
**************************************************************************/

PRIVATE int pqueue_transfer(proc_nr, opcode, position, iov, nr_req)
    int proc_nr;
    int opcode;
    u64_t position;
    iovec_t *iov;
    unsigned nr_req;
{
    int bytes, ret;
    int bytes_in; 	/* Bytes to insert */
    int bytes_out; 	/* Omitted bytes */
    int elem_read; 	/* Elements to read */
    int send_bytes;	/* Bytes to send */
    int i, j; 		/* Iterators */
    int elem_nr;	/* Number of elements to insert */

    char element_in[8];	/* Buffer for inserted elements */
    char * element_out;
    
    bytes = iov->iov_size;

    switch (opcode)
    {
        case DEV_SCATTER_S:
            bytes_out = bytes % 8;
            bytes_in = bytes - bytes_out;
            elem_nr = bytes / 8;

            for (i = 0; i < elem_nr; i++) {
               	if ((ret = sys_safecopyfrom(proc_nr, iov->iov_addr,
            	    i*8, (vir_bytes) element_in, 8, D)) != OK ) {
            		return ret;
            	}

            	iov->iov_size -= 8;
            	pqueue_add_element(element_in);
            }

            iov->iov_size -= bytes % 8;

            if (proc_counter > 0) {
            	if (dummy_process->next != NULL) {
            		notify(dummy_process->next->proc_nr);
            	}
            }
            break;

        case DEV_GATHER_S:
        	elem_read = (int) ceil(iov->iov_size / 8.0);
        	if (elem_counter == 0) {
        		pqueue_add_waiting_proc(proc_nr, iov);
            	return SUSPEND;
        	}

            elem_read = min(elem_counter, (int) ceil(iov->iov_size / 8.0));

            bytes = iov->iov_size;

            if (elem_read * 8 < bytes) {
            	send_bytes = elem_read * 8;
            } else {
            	send_bytes = bytes;
            }

           	element_out = pqueue_get_some_elements(elem_read, send_bytes);

            ret = sys_safecopyto(proc_nr, iov->iov_addr, 0,
                (vir_bytes) element_out, send_bytes, D);
            iov->iov_size -= bytes;
            break;
   
        default:
            return EINVAL;
    }
    return ret;
}

/**************************************************************************
                              SEF FUNCTIONS
**************************************************************************/

PRIVATE int sef_cb_lu_state_save(int state)
{
    return OK;
}

PRIVATE int lu_state_restore()
{
    return OK;
}

PRIVATE void sef_local_startup()
{
    sef_setcb_init_fresh(sef_cb_init);
    sef_setcb_init_lu(sef_cb_init);
    sef_setcb_init_restart(sef_cb_init);

    sef_startup();
}

PRIVATE int sef_cb_init(int type, sef_init_info_t *info)
{
    /* Initialize the pqueue driver */
    open_counter = 0;
    elem_counter = 0;
    proc_counter = 0;
    dummy_pqueue = (pqueue *) malloc(sizeof(pqueue));
    dummy_process = (process *) malloc(sizeof(process));

    /* Initialization completed successfully. */
    return OK;
}

/**************************************************************************
                              MAIN FUNCTION
**************************************************************************/

PUBLIC int main(int argc, char **argv)
{
    /* Initialization */
    sef_local_startup();

    /* Main loop */
    driver_task(&pqueue_tab, DRIVER_STD);
    return OK;
}
