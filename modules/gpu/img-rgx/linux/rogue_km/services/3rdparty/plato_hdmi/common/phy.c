/*************************************************************************/ /*!
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

@Copyright      Portions Copyright (c) Synopsys Ltd. All Rights Reserved
@License        Synopsys Permissive License

The Synopsys Software Driver and documentation (hereinafter "Software")
is an unsupported proprietary work of Synopsys, Inc. unless otherwise
expressly agreed to in writing between  Synopsys and you.

The Software IS NOT an item of Licensed Software or Licensed Product under
any End User Software License Agreement or Agreement for Licensed Product
with Synopsys or any supplement thereto.  Permission is hereby granted,
free of charge, to any person obtaining a copy of this software annotated
with this license and the Software, to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software,
and to permit persons to whom the Software is furnished to do so, subject
to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT     LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
DAMAGE.

*/ /**************************************************************************/

#include "img_defs.h"
#include "phy.h"
#include <linux/delay.h>

/*
 * See tables A.1 and B.1 in PHY datasheet
 */
#define MinPhyPixelClock    2500
#define MaxPhyPixelClock    59400

// TODO Generate table for > 8-bit pixel formats.
static const struct PhyGenericConfig
{
    IMG_UINT32  PixClock;
    IMG_UINT32  OPMODE_PLLCFG;
    IMG_UINT32  PLLCURRCTRL;
    IMG_UINT32  PLLGMPCTRL;
} gsPhyConfig_8bit[] =
{
    // PixelClock.    Reg: 0x06    0x10a   0x15
    { MinPhyPixelClock,   0x00B3, 0x0018, 0x0000 },
    {  4950,              0x0072, 0x0028, 0x0001 },
    {  9450,              0x0051, 0x0038, 0x0002 },
    { 18562,              0x0040, 0x0038, 0x0003 },
    { 34850,              0x1A40, 0x0018, 0x0003 },
    { MaxPhyPixelClock+1, 0x1A40, 0x0018, 0x0003 },    // last entry is invalid and must be > Max allowed
};
static const IMG_UINT32 NumPhyConfigs = ARRAY_SIZE(gsPhyConfig_8bit);

#define PHY_POLL(base, offset, mask, error) \
    { \
        IMG_UINT16 timeout = 0; \
        while (!(HDMI_READ_CORE_REG(base, offset) & mask) && \
            timeout < HDMI_PHY_TIMEOUT_MS) \
        { \
            DC_OSDelayus(HDMI_PHY_POLL_INTERVAL_MS * 1000); \
            timeout += HDMI_PHY_POLL_INTERVAL_MS; \
        } \
        if (timeout == HDMI_PHY_TIMEOUT_MS) \
        { \
            HDMI_ERROR_PRINT("- %s: PHY timed out polling on 0x%x in register offset 0x%x\n", __func__, mask, offset); \
            goto error; \
        } \
    }

bool PhyIsPclkSupported(IMG_UINT16 pixelClock)
{
    return ((pixelClock >= MinPhyPixelClock) && (pixelClock <= MaxPhyPixelClock));
}

#if defined(READ_HDMI_PHY_REGISTERS)
static IMG_UINT16 PhyI2CRead(IMG_CPU_VIRTADDR base, IMG_UINT8 addr)
{
    IMG_UINT16 data;

    HDMI_WRITE_CORE_REG(base, HDMI_PHY_I2CM_SLAVE_OFFSET, HDMI_PHY_SLAVE_ADDRESS);
    HDMI_WRITE_CORE_REG(base, HDMI_PHY_I2CM_ADDRESS_OFFSET, addr);
    HDMI_WRITE_CORE_REG(base, HDMI_PHY_I2CM_SLAVE_OFFSET, HDMI_PHY_SLAVE_ADDRESS);
    HDMI_WRITE_CORE_REG(base, HDMI_PHY_I2CM_OPERATION_OFFSET,
        SET_FIELD(HDMI_PHY_I2CM_OPERATION_RD_START, HDMI_PHY_I2CM_OPERATION_RD_MASK, 1));
    /* Wait for done indication */
    //PHY_POLL(base, HDMI_IH_I2CMPHY_STAT0_OFFSET, HDMI_IH_I2CMPHY_STAT0_I2CMPHYDONE_MASK, ERROR);
    mdelay(10);
    /* read the data registers */
    data  = (HDMI_READ_CORE_REG(base, HDMI_PHY_I2CM_DATAI_1_OFFSET) & 0xFF) << 8;
    data |= (HDMI_READ_CORE_REG(base, HDMI_PHY_I2CM_DATAI_0_OFFSET) & 0xFF);

    return data;
}
#endif // READ_HDMI_PHY_REGISTERS

PVRSRV_ERROR PhyI2CWrite(IMG_CPU_VIRTADDR base, IMG_UINT8 addr, IMG_UINT16 data)
{
    HDMI_CHECKPOINT;

    HDMI_WRITE_CORE_REG(base, HDMI_PHY_I2CM_SLAVE_OFFSET, HDMI_PHY_SLAVE_ADDRESS);
    HDMI_WRITE_CORE_REG(base, HDMI_PHY_I2CM_ADDRESS_OFFSET, addr);
    HDMI_WRITE_CORE_REG(base, HDMI_PHY_I2CM_DATAO_1_OFFSET, (data >> 8));
    HDMI_WRITE_CORE_REG(base, HDMI_PHY_I2CM_DATAO_0_OFFSET, data & 0xFF);
    HDMI_WRITE_CORE_REG(base, HDMI_PHY_I2CM_OPERATION_OFFSET,
        SET_FIELD(HDMI_PHY_I2CM_OPERATION_WR_START, HDMI_PHY_I2CM_OPERATION_WR_MASK, 1));

    /* Wait for done interrupt */
    PHY_POLL(base, HDMI_IH_I2CMPHY_STAT0_OFFSET, HDMI_IH_I2CMPHY_STAT0_I2CMPHYDONE_MASK, ERROR);

    // TODO: Check why pdump writes back to done and error bits
    HDMI_WRITE_CORE_REG(base, HDMI_IH_I2CMPHY_STAT0_OFFSET, 0x3);

    return PVRSRV_OK;

    ERROR:
        return PVRSRV_ERROR_TIMEOUT;
}

/*
Configure PHY based on pixel clock, color resolution, pixel repetition, and PHY model

NOTE: This assumes PHY model TSMC 28-nm HPM/ 1.8V
*/
void PhyConfigureMode(HDMI_DEVICE * pvDevice)
{
    IMG_UINT16 pixelClock;
    IMG_UINT8 colorRes;

    HDMI_CHECKPOINT;

    /* Init slave address */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_PHY_I2CM_SLAVE_OFFSET, HDMI_PHY_SLAVE_ADDRESS);

    pixelClock = pvDevice->videoParams.mDtdList[pvDevice->videoParams.mDtdActiveIndex].mPixelClock;
    colorRes = pvDevice->videoParams.mColorResolution;

    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_PHY_CONF0_OFFSET,
        SET_FIELD(HDMI_PHY_CONF0_SELDATAENPOL_START, HDMI_PHY_CONF0_SELDATAENPOL_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_ENHPDRXSENSE_START, HDMI_PHY_CONF0_ENHPDRXSENSE_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_PDDQ_START, HDMI_PHY_CONF0_PDDQ_MASK, 1));

    // TODO: What address is 0x13?? not defined in PHY spec
    //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x0000, 0x13); /* PLLPHBYCTRL */
    //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCLKBISTPHASE_OFFSET, 0x0006);
    /* RESISTANCE TERM 133Ohm Cfg  */
    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_TXTERM_OFFSET, 0x0004); /* TXTERM */
    /* REMOVE CLK TERM */
    //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKCALCTRL_OFFSET, 0x8000); /* CKCALCTRL */

    HDMI_DEBUG_PRINT(" - %s: pixel Clock: %d, colorResolution: %d\n", __func__, pixelClock, colorRes);

    if ((pixelClock < MinPhyPixelClock) || (pixelClock > MaxPhyPixelClock))
    {
        HDMI_ERROR_PRINT("- %s: pixel clock %d out of range [%d,%d]", __func__, pixelClock, MinPhyPixelClock, MaxPhyPixelClock);
        return;
    }

    // For 8-bit color formats look up the register values in table based on PHY datasheet.
    if (colorRes == 8)
    {
        IMG_UINT32 CKSYMTXCTRL;
        IMG_UINT32 VLEVCTRL;
        IMG_UINT32 ipix;
        for (ipix  = 0; ipix < NumPhyConfigs-1; ++ipix)
        {
            if (gsPhyConfig_8bit[ipix+1].PixClock > pixelClock)
                break;
        }
        if (ipix == NumPhyConfigs-1)
        {
            HDMI_ERROR_PRINT("- %s: pixel clock %d > MaxPhyPixelClock %d", __func__, pixelClock, MaxPhyPixelClock);
            return;
        }
        PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, gsPhyConfig_8bit[ipix].OPMODE_PLLCFG);
        PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   gsPhyConfig_8bit[ipix].PLLCURRCTRL);
        PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    gsPhyConfig_8bit[ipix].PLLGMPCTRL);

        // Clock Symbol and Transmitter Control Register 0x09
        if (pixelClock*10 < 29700000)       // 2.97GHz
            CKSYMTXCTRL = 0x8009;
        else if (pixelClock*10 < 34000000)  // 3.4GHz
            CKSYMTXCTRL = 0x8019;
        else
            CKSYMTXCTRL = 0x8029;
        PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET, CKSYMTXCTRL);

        // Voltage Level Control Register 0x0E
        if (pixelClock*10 < 14850000)       // 1.485GHz
            VLEVCTRL = 0x0232;
        else if (pixelClock*10 < 22200000)  // 2.22GHz
            VLEVCTRL = 0x0230;
        else if (pixelClock*10 < 34000000)  // 3.4GHz
            VLEVCTRL = 0x0273;
        else
            VLEVCTRL = 0x014A;
        PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET, VLEVCTRL);
    }
    else    // old code for > 8-bit. Needs to be modified as for 8-bit.
    switch (pixelClock)
    {
        // See dwc_hdmi20_tx_ns_6gbps_tsmc28hpm18_databook.pdf
		case 2520:
			switch (colorRes)
			{
				case 8:
					/* PLL/MPLL Cfg */
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x00b3);
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0018); /* CURRCTRL */
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0000); /* GMPCTRL */
					break;
				case 10: /* TMDS = 31.50MHz */
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x2153);
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0018);
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0000);
					break;
				case 12: /* TMDS = 37.80MHz */
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x40F3);
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0018);
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0000);
					break;
				case 16: /* TMDS = 50.40MHz */
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x60B2);
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0028);
					PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0001);
					break;
				default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
        			break;
			}
			/* PREEMP Cgf 0.00 */
			//PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET, 0x8009); /* CKSYMTXCTRL */
			/* TX/CK LVL 10 */
			//PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET, 0x0251); /* VLEVCTRL */
			break;
        case 2700:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x00B3);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0018);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0000);
	                break;
	            case 10: /* TMDS = 33.75MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x2153);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0018);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0000);
	                break;
	            case 12: /* TMDS = 40.50MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x40F3);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0018);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0000);
	                break;
	            case 16: /* TMDS = 54MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x60B2);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
				default:
	                HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
	    			break;

            }
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x8009, 0x09);
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x0251, 0x0E);
            break;
        case 5040:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x0072);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 10: /* TMDS = 63MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x2142);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 12: /* TMDS = 75.60MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x40A2);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 16: /* TMDS = 100.80MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x6071);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0002);
	                break;
	            default:
	                HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
	                break;

            }
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x8009, 0x09);
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x0251, 0x0E);
            break;
        case 5400:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x0072);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 10: /* TMDS = 67.50MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x2142);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 12: /* TMDS = 81MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x40A2);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 16: /* TMDS = 108MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x6071);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0002);
	                break;
	            default:
	                HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
	                break;

            }
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x8009, 0x09);
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x0251, 0x0E);
            break;
        case 5940:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x0072);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 10: /* TMDS = 74.25MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x2142);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 12: /* TMDS = 89.10MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x40A2);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 16: /* TMDS = 118.80MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x6071);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0002);
	                break;
                default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;

            }
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x8009, 0x09);
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x0251, 0x0E);
            break;
        case 7200:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x0072);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 10: /* TMDS = 90MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x2142);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0028);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0001);
	                break;
	            case 12: /* TMDS = 108MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x4061);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0002);
	                break;
	            case 16: /* TMDS = 144MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET,  0x6071);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,    0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,     0x0002);
	                break;
                default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;

            }
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x8009, 0x09);
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, 0x0251, 0x0E);
            break;
        /* 74.25 MHz pixel clock (720p)*/
        case 7425:
            switch (colorRes)
            {
                case 8:
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x0072);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET, 0x0028);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET, 0x0001);
                    break;
                case 10: /* TMDS = 92.812MHz */
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x2145);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET, 0x0038);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET, 0x0002);
                    break;
                case 12: /* TMDS = 111.375MHz */
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x4061);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET, 0x0038);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET, 0x0002);
                    break;
                case 16: /* TMDS = 148.5MHz */
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x6071);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET, 0x0038);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET, 0x0002);
                    break;
                default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;
            }
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET, 0x8009);
            //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET, 0x0251);
            break;

        case 10080:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x0051);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
	                break;
	            case 10: /* TMDS = 126MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x2145);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
	                break;
	            case 12: /* TMDS = 151.20MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x4061);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
	                break;
	            case 16: /* TMDS = 201.60MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x6050);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0003);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0211);
	                break;
                default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;
            }
            break;
        case 10100:
        case 10225:
        case 10650:
        case 10800:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x0051);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                //PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
	                break;
	            case 10: /* TMDS = 135MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x2145);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
	                break;
	            case 12: /* TMDS = 162MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x4061);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0211);
	                break;
	            case 16: /* TMDS = 216MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x6050);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0003);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0211);
	                break;
                default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;
            }
            break;
        case 11880:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x0051);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
	                break;
	            case 10: /* TMDS = 148.50MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x2145);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
	                break;
	            case 12: /* TMDS = 178.20MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x4061);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0211);
	                break;
	            case 16: /* TMDS = 237.60MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x6050);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0003);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0211);
	                break;
                default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;
            }
            break;

        case 14400:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x0051);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
	                break;
	            case 10: /* TMDS = 180MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x2145);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0211);
	                break;
	            case 12: /* TMDS = 216MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x4064);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0211);
	                break;
	            case 16: /* TMDS = 288MHz */
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x6050);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0003);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0211);
	                break;
                default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;
            }
            break;
        case 14625:
            switch (colorRes)
            {
	            case 8:
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x0051);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009);
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
	                break;
                default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;
            }
            break;
        case 14850:
    		switch (colorRes)
    		{
                case 8:
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x0051);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
                    break;
                case 10: /* TMDS = 185.62MHz */
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x214C);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET, 0x0038);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET, 0x0003);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET, 0x0211);
                    break;
                case 12: /* TMDS = 222.75MHz */
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x4064);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET, 0x0038);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET, 0x0003);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET, 0x0211);
                    break;
                case 16: /* TMDS = 297MHz */
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x6050);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET, 0x0038);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET, 0x0003);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET, 0x0273);
                    break;
                default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
        			break;
            }
            PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET, 0x8009);
            break;
        case 15400:
            switch (colorRes)
            {
            	case 8:
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x0051);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0002);
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251);
                    break;
				default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;
            }
            break;
        case 24150:
            switch (colorRes)
            {
            	case 8:
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_OPMODE_PLLCFG_OFFSET, 0x0040); // section 11.2.7 and table B.1
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLCURRCTRL_OFFSET,   0x0038); // section 11.2.17 and table B.1
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_PLLGMPCTRL_OFFSET,    0x0003); // section 11.2.20 and table B.1
                    PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_VLEVCTRL_OFFSET,      0x0251); // section 11.2.15 and table A.1
	                PhyI2CWrite(pvDevice->pvHDMIRegCpuVAddr, PHY_CKSYMTXCTRL_OFFSET,   0x8009); // section 11.2.10
                    break;
				default:
                    HDMI_ERROR_PRINT("- %s: Color depth not supported (%d)", __func__, colorRes);
                    break;
            }
            break;
        default:
            HDMI_ERROR_PRINT("- %s: \n\n\n*****Unsupported pixel clock ******\n\n\n!", __func__);
            break;
    }

    {
    	IMG_UINT32 value = HDMI_READ_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_PHY_CONF0_OFFSET);
        value = SET_FIELD(HDMI_PHY_CONF0_SELDATAENPOL_START, HDMI_PHY_CONF0_SELDATAENPOL_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_ENHPDRXSENSE_START, HDMI_PHY_CONF0_ENHPDRXSENSE_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_TXPWRON_START, HDMI_PHY_CONF0_TXPWRON_MASK, 1) |
        SET_FIELD(HDMI_PHY_CONF0_SVSRET_START, HDMI_PHY_CONF0_SVSRET_MASK, 1);
        HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_PHY_CONF0_OFFSET, value);
    }

#if defined(READ_HDMI_PHY_REGISTERS)
    {
        static IMG_UINT8 addresses[] =
        {
            PHY_OPMODE_PLLCFG_OFFSET,
            PHY_PLLCURRCTRL_OFFSET,
            PHY_PLLGMPCTRL_OFFSET,
            PHY_VLEVCTRL_OFFSET,
            PHY_CKSYMTXCTRL_OFFSET,
        };
        IMG_UINT16 data;
        IMG_UINT32 n;
        for (n = 0; n < sizeof(addresses); ++n)
        {
            data = PhyI2CRead(pvDevice->pvHDMIRegCpuVAddr, addresses[n]);
            printk(KERN_INFO "HDMI I2 addr 0x%02x = 0x%04x\n", addresses[n], data);
        }
    }
#endif // READ_HDMI_PHY_REGISTERS
}

void PhyReset(HDMI_DEVICE * pvDevice, IMG_UINT8 value)
{
    /* Handle different types of PHY here... */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_MC_PHYRSTZ_OFFSET, value & 0x1);
    msleep(HDMI_PHY_RESET_TIME_MS);
}

PVRSRV_ERROR PhyPowerDown(HDMI_DEVICE * pvDevice)
{
    /* For HDMI TX 1.4 PHY, power down by placing PHY in reset */
    PhyReset(pvDevice, 0);
    PhyReset(pvDevice, 1);
    PhyReset(pvDevice, 0);
    PhyReset(pvDevice, 1);
    return PVRSRV_OK;
}

PVRSRV_ERROR PhyWaitLock(HDMI_DEVICE * pvDevice)
{
    PHY_POLL(pvDevice->pvHDMIRegCpuVAddr, HDMI_PHY_STAT0_OFFSET, HDMI_PHY_STAT0_TX_PHY_LOCK_MASK, ERROR);
    return PVRSRV_OK;

    ERROR:
    return PVRSRV_ERROR_TIMEOUT;
}

PVRSRV_ERROR PhyInitialize(HDMI_DEVICE * pvDevice)
{

    /* Init slave address */
    HDMI_WRITE_CORE_REG(pvDevice->pvHDMIRegCpuVAddr, HDMI_PHY_I2CM_SLAVE_OFFSET, HDMI_PHY_SLAVE_ADDRESS);

    return PVRSRV_OK;
}

