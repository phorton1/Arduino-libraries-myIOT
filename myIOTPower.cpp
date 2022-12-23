
#include "myIOTPower.h"

#if WITH_POWER


//--------------------------------
// power monitor
//--------------------------------

#include "myIOTDevice.h"
#include "myIOTLog.h"
#include <esp_adc_cal.h>


#define DEBUG_POWER  0      // upto 2

// if WITH_WS is not defined and DEBUG_POWER==0, the task is not started


// calibration values at 12.5V
// and approximate current consumption

#define VOLT_CALIB_OFFSET  0.50     // 0.22
#define AMP_CALIB_OFFSET   0.025     // -0.03

#define VOLTAGE_DIVIDER_RATIO    ((4640.0 + 1003.0)/1003.0)       // (R1 + R2)/R2   ~5.6, so 3.3V==18.5V
    // "4.72K" and "1K" == approx 3 ma through to ground at 12.5V
#define CURRENT_DIVIDER_RATIO      ((9410.0 + 9410.0)/9410.0)       // (R1 + R2)/R2   ~2, so 3.3V==6.6V (we need 0..5)                                       ))
    // negligble 0.263 ma current to ground

#define CIRC_BUF_SIZE   100

int volt_buf[CIRC_BUF_SIZE];
int amp_buf[CIRC_BUF_SIZE];
int volt_buf_num = 0;
int amp_buf_num = 0;



static void powerTask(void *param)
{
    delay(1650);
    LOGI("starting powerTask() loop on core(%d)",xPortGetCoreID());

    while (1)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);    // I believe this is vTaskDelay(1) = 10ms
        volt_buf[volt_buf_num++] = analogRead(PIN_MAIN_VOLTAGE);
        if (volt_buf_num >= CIRC_BUF_SIZE) volt_buf_num = 0;
        amp_buf[amp_buf_num++] = analogRead(PIN_MAIN_CURRENT);
        if (amp_buf_num >= CIRC_BUF_SIZE) amp_buf_num = 0;
    }
}


void initPower()
{
    #if defined(WITH_WS) || DEBUG_POWER
        LOGI("starting powerTask");
        xTaskCreate(powerTask,
            "powerTask",
            2048,
            NULL,   // param
            1,  	// priority
            NULL);  // *handle
    #else
        LOGI("WITH_POWER is defined, but WITH_WS is not and DEBUG_POWER==0, so NOT STARTING powerTask");
    #endif
}


static float getActualVolts(int *buf)
 {
    int val = 0;
    for (int i=0; i<CIRC_BUF_SIZE; i++)
    {
        val += buf[i];
    }
    val /= CIRC_BUF_SIZE;
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1200, &adc_chars);
        // prh dunno what it does, but it's way better than analogRead()/4096 * 3.3V
        // The 1100 is from https://deepbluembedded.com/esp32-adc-tutorial-read-analog-voltage-arduino/
        // The constant does not seem to do anything
    uint32_t raw_millis = esp_adc_cal_raw_to_voltage(val, &adc_chars);
    return (float) raw_millis/1000.0;
}


static float getMainVoltage()
{
    float raw_volts = getActualVolts(volt_buf);
    float undivided_volts = raw_volts * VOLTAGE_DIVIDER_RATIO;
    float final_volts = undivided_volts + VOLT_CALIB_OFFSET;
    #if DEBUG_POWER > 1
        LOGD("MAIN    raw=%-1.3f   undiv=%-2.3f   final=%-2.3f",
            raw_volts,
            undivided_volts,
            final_volts);
    #endif
    return final_volts;
}


static float getMainCurrent()
{
    float raw_volts = getActualVolts(amp_buf);
    float undivided_volts = raw_volts * CURRENT_DIVIDER_RATIO;
    float biased_volts = undivided_volts - 2.5;
    float current = (biased_volts/2.5) * 5.0;
    float final_current = current + AMP_CALIB_OFFSET;
    #if DEBUG_POWER > 1
        LOGD("CURRENT raw=%-1.3f   undiv=%-2.3f   biased=%-2.3f   amps=%-2.3f   final=%-2.3f",
            raw_volts,
            undivided_volts,
            biased_volts,
            current,
            final_current);
    #endif
    return final_current;
}




static void powerLoopBody()
{
    uint32_t now = millis();
    static uint32_t check_time =0;
    if (now > check_time + 1000)
    {
        check_time = now;
        float main_volts = getMainVoltage();
        float main_amps = getMainCurrent();

        #if WITH_WS
            #if DEBUG_POWER
                if (my_iot_device->getConnectStatus() == IOT_CONNECT_STA)
            #endif
            {
                my_iot_device->setFloat(ID_DEVICE_VOLTS, main_volts);
                my_iot_device->setFloat(ID_DEVICE_AMPS,  main_amps);

                // String msg = "{\"power_volts\":";
                // msg += String(main_volts,2);
                // msg += ",\"power_amps\":";
                // msg += String(main_amps,3);
                // msg += "}";
                // my_iot_device->wsBroadcast(msg.c_str());
            }
        #endif

        #if DEBUG_POWER
            LOGD("POWER %0.2fV %0.3fA",main_volts,main_amps);
        #endif
    }
}


void powerLoop()
{
    #if !DEBUG_POWER
        #if WITH_WS
            if (my_iot_device->getConnectStatus() == IOT_CONNECT_STA)
                powerLoopBody();
        #endif
        // nothing if !DEBUG_POWER && !defined(WITH_WS)
    #else
        // always do the body if DEBUG_POWER
        powerLoopBody();
    #endif
}



#endif  // WITH_POWER
