//--------------------------------------------------------------------------------------------------
/**
 * Periodic sensor scaffold.  Used to implement generic periodic sensors.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"


//--------------------------------------------------------------------------------------------------
/**
 * Sensor Scaffold object.
 */
//--------------------------------------------------------------------------------------------------
typedef struct psensor
{
    bool isEnabled;
    double period;  ///< seconds (0.0 = not set yet)
    le_timer_Ref_t timer;
    void (*sampleFunc)(psensor_Ref_t, void *);
    void *sampleFuncContext;
    char name[PSENSOR_MAX_NAME_BYTES];

    dhubIO_TriggerPushHandlerRef_t triggerHandlerRef;
    dhubIO_NumericPushHandlerRef_t periodHandlerRef;
    dhubIO_BooleanPushHandlerRef_t enableHandlerRef;
}
Sensor_t;


//--------------------------------------------------------------------------------------------------
/**
 * Pool from which Sensor_t objects are allocated.
 */
//--------------------------------------------------------------------------------------------------
static le_mem_PoolRef_t SensorPool = NULL;


//--------------------------------------------------------------------------------------------------
/**
 * Safe references to lookup Sensor_t objects.
 *
 * @note The size of the map doesn't have to be very big, since the map is located in the client
 * i.e. there is a separate map for each client.  Use a prime number!
 */
//--------------------------------------------------------------------------------------------------
static le_ref_MapRef_t SensorMap;
#define PSENSOR_SAFEREF_MAP_SIZE  7


//--------------------------------------------------------------------------------------------------
/**
 * Sensor lookup function.
 */
//--------------------------------------------------------------------------------------------------
static Sensor_t *LookupSensor
(
    void *ref
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t *sensorPtr = (Sensor_t *)le_ref_Lookup(SensorMap, ref);

    if (NULL == sensorPtr)
    {
        LE_ERROR("No sensor found for ref %p", ref);
    }

    return sensorPtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Timer expiry handler function.
 */
//--------------------------------------------------------------------------------------------------
static void HandleTimerExpiry
(
    le_timer_Ref_t timer
)
//--------------------------------------------------------------------------------------------------
{
    void *sensorRef = le_timer_GetContextPtr(timer);
    Sensor_t* sensorPtr = LookupSensor(sensorRef);

    if (NULL != sensorPtr)
    {
        sensorPtr->sampleFunc(sensorRef, sensorPtr->sampleFuncContext);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle an "enable" update from the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void HandleEnablePush
(
    double timestamp,   ///< Don't care about this.
    bool enable,
    void* sensorRef
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = LookupSensor(sensorRef);

    if (NULL != sensorPtr)
    {
        if (sensorPtr->isEnabled != enable)
        {
            sensorPtr->isEnabled = enable;

            if (enable)
            {
                // If the period has been set, take a sample and start the timer.
                if (sensorPtr->period > 0.0)
                {
                    sensorPtr->sampleFunc(sensorRef, sensorPtr->sampleFuncContext);
                    le_timer_Start(sensorPtr->timer);
                }
            }
            else
            {
                le_timer_Stop(sensorPtr->timer);
            }
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a "period" update from the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void HandlePeriodPush
(
    double timestamp,   ///< Don't care about this.
    double period,      ///< seconds
    void* sensorRef
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = LookupSensor(sensorRef);

    if (NULL != sensorPtr)
    {
        // If the new value is the same as the old value, ignore the push.
        if (sensorPtr->period != period)
        {
            // Sanity check the period.
            // If it's invalid, stop the timer and set the period to 0.0.
            if (period <= 0.0)
            {
                LE_ERROR("Timer period %lf is out of range. Must be > 0.", period);
                le_timer_Stop(sensorPtr->timer);
                sensorPtr->period = 0.0;
            }
            else if (period > (double)(0x7FFFFFFF)) // Don't know how big time_t is, assume 32-bits.
            {
                LE_ERROR("Timer period %lf is too high.", period);
                le_timer_Stop(sensorPtr->timer);
                sensorPtr->period = 0.0;
            }
            else
            {
                // The new period is good.
                // Set the timer's interval to the period.
                le_clk_Time_t interval;
                interval.sec = (time_t)period;
                interval.usec = (period - interval.sec) * 1000000;
                le_timer_SetInterval(sensorPtr->timer, interval);

                // If the old value was zero and the sensor is enabled, take a sample and
                // start the timer now.
                if ((sensorPtr->period == 0) && sensorPtr->isEnabled)
                {
                    sensorPtr->sampleFunc(sensorRef, sensorPtr->sampleFuncContext);
                    le_timer_Start(sensorPtr->timer);
                }

                sensorPtr->period = period;
            }
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a "trigger" update from the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void HandleTriggerPush
(
    double timestamp,   ///< Don't care about this.
    void* sensorRef
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = LookupSensor(sensorRef);

    if (NULL != sensorPtr)
    {
        if (sensorPtr->isEnabled)
        {
            sensorPtr->sampleFunc(sensorRef, sensorPtr->sampleFuncContext);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Build up the path to a resource from the sensor name and the resource (leaf) name.
 */
//--------------------------------------------------------------------------------------------------
static void BuildResourcePath
(
    char* pathBuffPtr,  ///< Ptr to the buffer into which the path will be built.
    size_t pathBuffSize,    ///< Size (in bytes) of the buffer pointed to by pathBuffPtr.
    Sensor_t* sensorPtr, ///< Ptr to the Sensor scaffold object.
    const char* resourceName  ///< E.g., "value" or "enable"
)
//--------------------------------------------------------------------------------------------------
{
    if (sensorPtr->name[0] == '\0')
    {
        LE_ASSERT(LE_OK == le_utf8_Copy(pathBuffPtr, resourceName, pathBuffSize, NULL));
    }
    else
    {
        LE_ASSERT(snprintf(pathBuffPtr, pathBuffSize, "%s/%s", sensorPtr->name, resourceName)
                  < pathBuffSize);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Creates a periodic sensor scaffold for a sensor with a given name.
 *
 * This makes the sensor appear in the Data Hub and creates a timer for that sensor.
 * The sampleFunc will be called whenever it's time to take a sample.  The sampleFunc must
 * call one of the psensor_PushX() functions below.
 *
 * @return Reference to the new periodic sensor scaffold.
 */
//--------------------------------------------------------------------------------------------------
psensor_Ref_t psensor_Create
(
    const char* name,   ///< Name of the periodic sensor, or "" if the app name is sufficient.
    dhubIO_DataType_t dataType,
    const char* units,
    void (*sampleFunc)(psensor_Ref_t ref,
                       void *context), ///< Sample function to be called back periodically.
    void *sampleFuncContext  ///< Context pointer to be passed to the sample function
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = le_mem_ForceAlloc(SensorPool);
    void *sensorRef = le_ref_CreateRef(SensorMap, sensorPtr);

    sensorPtr->isEnabled = false;
    sensorPtr->period = 0.0;

    sensorPtr->timer = le_timer_Create(name);
    le_timer_SetRepeat(sensorPtr->timer, 0); // Repeat an infinite number of times.
    le_timer_SetHandler(sensorPtr->timer, HandleTimerExpiry);
    le_timer_SetContextPtr(sensorPtr->timer, sensorRef);

    sensorPtr->sampleFunc = sampleFunc;
    sensorPtr->sampleFuncContext = sampleFuncContext;

    if (le_utf8_Copy(sensorPtr->name, name, sizeof(sensorPtr->name), NULL) != LE_OK)
    {
        LE_FATAL("Sensor name too long (%s)", name);
    }

    // Create the Data Hub resources "value", "enable", "period", and "trigger" for this sensor.
    char path[DHUBIO_MAX_RESOURCE_PATH_LEN];
    BuildResourcePath(path, sizeof(path), sensorPtr, "value");
    le_result_t result = dhubIO_CreateInput(path, dataType, units);
    if (result != LE_OK)
    {
        LE_FATAL("Failed to create Data Hub Input '%s' (%s).", path, LE_RESULT_TXT(result));
    }

    BuildResourcePath(path, sizeof(path), sensorPtr, "enable");
    result = dhubIO_CreateOutput(path, DHUBIO_DATA_TYPE_BOOLEAN, "");
    if (result != LE_OK)
    {
        LE_FATAL("Failed to create Data Hub Output '%s' (%s).", path, LE_RESULT_TXT(result));
    }
    sensorPtr->enableHandlerRef = dhubIO_AddBooleanPushHandler(path, HandleEnablePush, sensorRef);

    BuildResourcePath(path, sizeof(path), sensorPtr, "period");
    result = dhubIO_CreateOutput(path, DHUBIO_DATA_TYPE_NUMERIC, "s");
    if (result != LE_OK)
    {
        LE_FATAL("Failed to create Data Hub Output '%s' (%s).", path, LE_RESULT_TXT(result));
    }
    sensorPtr->periodHandlerRef = dhubIO_AddNumericPushHandler(path, HandlePeriodPush, sensorRef);

    BuildResourcePath(path, sizeof(path), sensorPtr, "trigger");
    result = dhubIO_CreateOutput(path, DHUBIO_DATA_TYPE_TRIGGER, "");
    if (result != LE_OK)
    {
        LE_FATAL("Failed to create Data Hub Output '%s' (%s).", path, LE_RESULT_TXT(result));
    }
    sensorPtr->triggerHandlerRef = dhubIO_AddTriggerPushHandler(path, HandleTriggerPush, sensorRef);
    dhubIO_MarkOptional(path);

    return (psensor_Ref_t)sensorRef;
}


//--------------------------------------------------------------------------------------------------
/**
 * Creates a periodic sensor scaffold for a sensor with a given name that produces JSON samples.
 *
 * This makes the sensor appear in the Data Hub and creates a timer for that sensor.
 * The sampleFunc will be called whenever it's time to take a sample.  The sampleFunc is supposed
 * to call psensor_PushJson() to push the JSON sample.
 *
 * @return Reference to the new periodic sensor scaffold.
 */
//--------------------------------------------------------------------------------------------------
psensor_Ref_t psensor_CreateJson
(
    const char* name,   ///< Name of the periodic sensor, or "" if the app name is sufficient.
    const char* jsonExample,    ///< String containing example JSON value.
    void (*sampleFunc)(psensor_Ref_t ref,
                       void *context), ///< Sample function to be called back periodically.
    void *sampleFuncContext  ///< Context pointer to be passed to the sample function
)
//--------------------------------------------------------------------------------------------------
{
    psensor_Ref_t ref = psensor_Create(name,
                                       DHUBIO_DATA_TYPE_JSON,
                                       "", // units
                                       sampleFunc,
                                       sampleFuncContext);

    char path[DHUBIO_MAX_RESOURCE_PATH_LEN];
    Sensor_t* sensorPtr = LookupSensor(ref);
    LE_ASSERT(sensorPtr);
    BuildResourcePath(path, sizeof(path), sensorPtr, "value");
    dhubIO_SetJsonExample(path, jsonExample);

    return ref;
}


//--------------------------------------------------------------------------------------------------
/**
 * Removes a periodic sensor scaffold and all associated resources
 */
//--------------------------------------------------------------------------------------------------
void psensor_Destroy
(
    psensor_Ref_t *ref
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr;
    char path[DHUBIO_MAX_RESOURCE_PATH_LEN];

    LE_ASSERT(NULL != ref);

    sensorPtr = LookupSensor(*ref);
    le_ref_DeleteRef(SensorMap, *ref);

    *ref = NULL;

    if (sensorPtr)
    {
        sensorPtr->isEnabled = false;

        // Stop and delete timer
        (void)le_timer_Stop (sensorPtr->timer);
        le_timer_Delete(sensorPtr->timer);

        // Deregister handlers and remove resources
        BuildResourcePath(path, sizeof(path), sensorPtr, "trigger");
        dhubIO_RemoveTriggerPushHandler(sensorPtr->triggerHandlerRef);
        dhubIO_DeleteResource(path);

        BuildResourcePath(path, sizeof(path), sensorPtr, "period");
        dhubIO_RemoveNumericPushHandler(sensorPtr->periodHandlerRef);
        dhubIO_DeleteResource(path);

        BuildResourcePath(path, sizeof(path), sensorPtr, "enable");
        dhubIO_RemoveBooleanPushHandler(sensorPtr->enableHandlerRef);
        dhubIO_DeleteResource(path);

        BuildResourcePath(path, sizeof(path), sensorPtr, "value");
        dhubIO_DeleteResource(path);

        le_mem_Release(sensorPtr);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a boolean sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
void psensor_PushBoolean
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    bool value
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = LookupSensor(ref);

    if (NULL != sensorPtr)
    {
        char path[DHUBIO_MAX_RESOURCE_PATH_LEN];
        LE_ASSERT(snprintf(path, sizeof(path), "%s/value", sensorPtr->name) < sizeof(path));

        dhubIO_PushBoolean(path, timestamp, value);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a numeric sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
void psensor_PushNumeric
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    double value
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = LookupSensor(ref);

    if (NULL != sensorPtr)
    {
        char path[DHUBIO_MAX_RESOURCE_PATH_LEN];
        LE_ASSERT(snprintf(path, sizeof(path), "%s/value", sensorPtr->name) < sizeof(path));

        dhubIO_PushNumeric(path, timestamp, value);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a string sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
void psensor_PushString
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    const char* value
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = LookupSensor(ref);

    if (NULL != sensorPtr)
    {
        char path[DHUBIO_MAX_RESOURCE_PATH_LEN];
        LE_ASSERT(snprintf(path, sizeof(path), "%s/value", sensorPtr->name) < sizeof(path));

        dhubIO_PushString(path, timestamp, value);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Push a JSON sample to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
void psensor_PushJson
(
    psensor_Ref_t ref,  ///< Reference returned by psensor_Create().
    double timestamp,   ///< Timestamp (or 0 = now).
    const char* value
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = LookupSensor(ref);

    if (NULL != sensorPtr)
    {
        char path[DHUBIO_MAX_RESOURCE_PATH_LEN];
        LE_ASSERT(snprintf(path, sizeof(path), "%s/value", sensorPtr->name) < sizeof(path));

        dhubIO_PushJson(path, timestamp, value);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize sensor pool and safe references
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    SensorPool = le_mem_CreatePool("psensor", sizeof(Sensor_t));

    SensorMap = le_ref_CreateMap("contexts", PSENSOR_SAFEREF_MAP_SIZE);
}
