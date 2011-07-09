/*
*       Console Service.
*
*       Copyright (C) 2002, 2003, 2004, 2005
*      
*       Santiago Bazerque       sbazerque@gmail.com                    
*       Nicolas de Galarreta    nicodega@gmail.com
*
*      
*       Redistribution and use in source and binary forms, with or without
*       modification, are permitted provided that conditions specified on
*       the License file, located at the root project directory are met.
*
*       You should have received a copy of the License along with the code,
*       if not, it can be downloaded from our project site: sartoris.sourceforge.net,
*       or you can contact us directly at the email addresses provided above.
*
*
*/

#include "csl_internals.h"

extern struct vterm t[NUM_VIRTUAL];

int mouse_suscriptions_ports[MAX_MOUSE_SUSCRIPTIONS];
int mouse_suscriptions[MAX_MOUSE_SUSCRIPTIONS];
int msuscs;

// this function will return non 0 if the service must die.
int process_stdservice(void)
{
    struct stdservice_cmd service_cmd;
    struct stdservice_res servres;
    int id_proc = -1;
    int die = 0;

    while (get_msg_count(STDSERVICE_PORT) > 0)
    {
        get_msg(STDSERVICE_PORT, &service_cmd, &id_proc);

        servres.ret = STDSERVICE_RESPONSE_OK;
        servres.command = service_cmd.command;

        if(service_cmd.command == STDSERVICE_DIE || service_cmd.command == STDSERVICE_FORCEDIE)
        {
                die = 1;
                dieres.ret = STDSERVICE_RESPONSE_OK;
                dieres.command = service_cmd.command;
                dieid = id_proc;
                dieretport = service_cmd.ret_port;

                break;
        }
        else if(service_cmd.command == STDSERVICE_QUERYINTERFACE)
                process_query_interface((struct stdservice_query_interface *)&service_cmd, id_proc);
        else
                send_msg(id_proc, service_cmd.ret_port, &servres);              
    }
    return die;
}

void process_query_interface(struct stdservice_query_interface *query_cmd, int task)
{
    struct stdservice_query_res qres;

    qres.command = STDSERVICE_QUERYINTERFACE;
    qres.ret = STDSERVICE_RESPONSE_FAILED;
    qres.msg_id = query_cmd->msg_id;

    switch(query_cmd->uiid)
    {
            case STDDEV_UIID:
                    // check version
                    if(query_cmd->ver != STDDEV_VERSION) break;
                    qres.port = CSL_STDDEV_PORT;
                    qres.ret = STDSERVICE_RESPONSE_OK;
                    break;
            case STD_CHAR_DEV_UIID:
                    if(query_cmd->ver != STD_CHAR_DEV_VER) break;
                    qres.port = CSL_STDCHARDEV_PORT;
                    qres.ret = STDSERVICE_RESPONSE_OK;
                    break;
            default:
                    break;
    }

    // send response
    send_msg(task, query_cmd->ret_port, &qres);
}

void process_stddev(void)
{
    struct stddev_cmd stddev_msg;
    struct stddev_res res;
    struct stddev_devtype_res *type_res = NULL;
    struct stddev_ver_res *ver_res = NULL;
    struct stddev_ioctl_cmd *ioctl = NULL;
    struct key_suscription *s = NULL;
    int i = 0, sender_id;  

    while(get_msg_count(CSL_STDDEV_PORT) > 0)
    {
            get_msg(CSL_STDDEV_PORT, &stddev_msg, &sender_id);

            res.logic_deviceid = -1;
            res.command = stddev_msg.command;
            res.msg_id = stddev_msg.msg_id;

            switch(stddev_msg.command)
            {
                    case STDDEV_GET_DEVICETYPE:
                            type_res = (struct stddev_devtype_res*)&res;
                            type_res->dev_type = STDDEV_CHAR;
                            type_res->ret = STDDEV_OK;
                            type_res->msg_id = stddev_msg.msg_id;
                            break;
                    case STDDEV_VER:
                            ver_res = (struct stddev_ver_res*)&res;
                            ver_res->ret = STDDEV_OK;
                            ver_res->ver = STDDEV_VERSION;
                            ver_res->msg_id = stddev_msg.msg_id;
                            break;
                    case STDDEV_GET_DEVICE:
                    case STDDEV_GET_DEVICEX:
                            res.logic_deviceid = stddev_msg.padding0;
                            res.msg_id = stddev_msg.msg_id;
                            res.ret = STDDEV_OK;
                            break;
                    case STDDEV_FREE_DEVICE:
                            res.logic_deviceid = stddev_msg.padding0;
                            res.msg_id = stddev_msg.msg_id;
                            res.ret = STDDEV_OK;
                            break;
                    case STDDEV_IOCTL:
                            ioctl = (struct stddev_ioctl_cmd*)&stddev_msg;
                            res.ret = STDDEV_ERR;
                            res.logic_deviceid = ioctl->logic_deviceid;
               
                            //if(!check_ownership(ioctl->logic_deviceid, sender_id)) break;
                            if(res.logic_deviceid >= NUM_VIRTUAL)
                                    string_print("CONS: ASSERT 1",0,7);

                            ((struct stddev_ioctrl_res *)&res)->dev_error = -1;
                            switch(ioctl->request)
                            {
                                    case CSL_IO_DISABLE_ECHO:
                                            i = 0;
                                            while(i < t[res.logic_deviceid].owners)
                                            {
                                                    if(t[res.logic_deviceid].ownerid[i] == sender_id)
                                                    {
                                                            t[res.logic_deviceid].force_echo[i] = CSL_FORCE_NOECHO;
                                                            break;
                                                    }
                                                    i++;
                                            }
                                            res.ret = STDDEV_OK;
                                            break;
                                    case CSL_IO_COMMAND_ECHO:
                                            i = 0;
                                            while(i < t[res.logic_deviceid].owners)
                                            {
                                                    if(t[res.logic_deviceid].ownerid[i] == sender_id)
                                                    {
                                                            t[res.logic_deviceid].force_echo[i] = CSL_FORCE_NONE;
                                                            break;
                                                    }
                                                    i++;
                                            }
                                            res.ret = STDDEV_OK;
                                            break;
                                    case CSL_IO_ENABLE_ECHO:
                                            i = 0;
                                            while(i < t[res.logic_deviceid].owners)
                                            {
                                                    if(t[res.logic_deviceid].ownerid[i] == sender_id)
                                                    {
                                                            t[res.logic_deviceid].force_echo[i] = CSL_FORCE_ECHO;
                                                            break;
                                                    }
                                                    i++;
                                            }
                                            res.ret = STDDEV_OK;
                                            break;
                                    case CSL_IO_SWITCH:
                                            vt_switch(ioctl->logic_deviceid);
                                            res.ret = STDDEV_OK;
                                            break;
                                    case CSL_IO_SETATT:
                                            t[ioctl->logic_deviceid].attribute = (char)ioctl->param;
                                            res.ret = STDDEV_OK;
                                            break;
                                    case CSL_IO_SETSIGNALPORT:
                                            s = get_suscription(sender_id, ioctl->logic_deviceid);
                    if(s)
                    {
                                                s->port = ioctl->param;
                                                res.ret = STDDEV_OK;
                    }
                    else
                    {
                            res.ret = STDDEV_ERR;
                    }
                                            break;
                case CSL_IO_SIGNAL:
                                            s = get_suscription(sender_id, ioctl->logic_deviceid);
                                            if(s == NULL || s->susc != MAX_SUSCRIPTIONS)
                                            {
                                                    res.ret = STDDEV_OK;
                                                    s->keycodes[s->susc] = (char)ioctl->param;
                                                    s->susc++;
                                            }
                    else
                    {
                            res.ret = STDDEV_ERR;
                    }
                                            break;
                                    case CSL_IO_USIGNAL:
                                            if(remove_suscription(sender_id, ioctl->logic_deviceid))
                                                    res.ret = STDDEV_ERR;
                                            else
                                                    res.ret = STDDEV_OK;
                                            break;
                case CSL_IO_MSIGNAL:
                    {
                        for(i = 0; i < MAX_MOUSE_SUSCRIPTIONS; i++)
                        {
                            if(mouse_suscriptions[i] == -1)
                                break;
                        }
                        if(i == MAX_MOUSE_SUSCRIPTIONS)
                        {
                            res.ret = STDDEV_ERR;
                        }
                        else
                        {
                            mouse_suscriptions[i] = sender_id;
                            mouse_suscriptions_ports[i] = 0;
                            msuscs++;
                            res.ret = STDDEV_OK;
                        }
                    }
                                            break;
                case CSL_IO_UMSIGNAL:
                    {
                        for(i = 0; i < MAX_MOUSE_SUSCRIPTIONS; i++)
                        {
                            if(mouse_suscriptions[i] == sender_id)
                                break;
                        }
                        if(i == MAX_MOUSE_SUSCRIPTIONS)
                        {
                            res.ret = STDDEV_ERR;
                        }
                        else
                        {
                            mouse_suscriptions[i] = -1;
                            msuscs--;
                            res.ret = STDDEV_OK;
                        }
                    }
                                            break;
                case CSL_IO_SETMSIGNALPORT:
                                            {
                        for(i = 0; i < MAX_MOUSE_SUSCRIPTIONS; i++)
                        {
                            if(mouse_suscriptions[i] == sender_id)
                                break;
                        }
                        if(i == MAX_MOUSE_SUSCRIPTIONS)
                        {
                            res.ret = STDDEV_ERR;
                        }
                        else
                        {
                            mouse_suscriptions_ports[i] = ioctl->param;
                            res.ret = STDDEV_OK;
                        }
                    }
                                            break;
                case CSL_MOUSE_ENABLE:
                    mouse_enable(ioctl->param);
                                            res.ret = STDDEV_OK;
                    break;
                            }
                    break;
            }
            send_msg(sender_id, stddev_msg.ret_port, &res);
    }
}

struct key_suscription *get_suscription(int sender_id, int console)
{
    CPOSITION it;
    struct key_suscription *s;

    it = get_head_position(&t[console].suscribers);

    while(it != NULL)
    {
            s = (struct key_suscription *)get_next(&it);
            if(s->taskid == sender_id) return s;
    }
       
    s = (struct key_suscription *)malloc(sizeof(struct key_suscription));

    if(s == NULL)
            return NULL;
       
    s->susc = 0;
    s->taskid = sender_id;
    s->port = 0;

    add_head(&t[console].suscribers, s);

    return s;
}

int remove_suscription(int sender_id, int console)
{
    CPOSITION it;
    struct key_suscription *s;

    it = get_head_position(&t[console].suscribers);

    while(it != NULL)
    {
        s = (struct key_suscription *)get_next(&it);
        if(s->taskid == sender_id)
        {
                remove_at(&t[console].suscribers, it);
                free(s);
                return 0; // success
        }
    }
    return 1;
}

void swap_array(int *array, int start, int length)
{
    int k = start;

    while(start < length)
    {
            array[k] = array[k + 1];
            k++;
    }

    if(length != 0) array[k] = -1;
}

