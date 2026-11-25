#ifndef MSGBUS_TOPIC_TABLE_H
#define MSGBUS_TOPIC_TABLE_H

#include "msgbus/protocol.h"

#define MAX_BROKER_TOPICS 64

typedef struct {
    char topic_name[MSGBUS_MAX_TOPIC_NAME];
    bool in_use;
    bool durable;
} broker_topic_entry_t;

void topic_table_init(void);
int  topic_table_add(const char *name);
int  topic_table_add_durable(const char *name, bool durable);
int  topic_table_remove(const char *name);
void topic_table_cleanup_all(void);

#endif // MSGBUS_TOPIC_TABLE_H
