#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/signal.h>
#include <linux/irq.h>
#include <linux/ctype.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <asm/rt2880/surfboardint.h>	/* for cp0 reg access, added by bobtseng */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/mca.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <linux/init.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#include "../../../../config/autoconf.h"

#if defined(CONFIG_USER_SNMPD)
#include <linux/seq_file.h>
#endif

#include "ra2882ethreg.h"
#include "raether.h"
#include "ra_mac.h"
#include "ra_ethtool.h"

extern struct net_device *dev_raether;


#if defined(CONFIG_USER_SNMPD)

static int ra_snmp_seq_show(struct seq_file *seq, void *v)
{
	char strprint[100];

#if !defined(CONFIG_RALINK_RT5350) && !defined(CONFIG_RALINK_RT6352)

	sprintf(strprint, "rx counters: %x %x %x %x %x %x %x\n", sysRegRead(GDMA_RX_GBCNT0), sysRegRead(GDMA_RX_GPCNT0),sysRegRead(GDMA_RX_OERCNT0), sysRegRead(GDMA_RX_FERCNT0), sysRegRead(GDMA_RX_SERCNT0), sysRegRead(GDMA_RX_LERCNT0), sysRegRead(GDMA_RX_CERCNT0));
	seq_puts(seq, strprint);

	sprintf(strprint, "fc config: %x %x %x %x\n", sysRegRead(CDMA_FC_CFG), sysRegRead(GDMA1_FC_CFG), PDMA_FC_CFG, sysRegRead(PDMA_FC_CFG));
	seq_puts(seq, strprint);

	sprintf(strprint, "scheduler: %x %x %x\n", sysRegRead(GDMA1_SCH_CFG), sysRegRead(GDMA2_SCH_CFG), sysRegRead(PDMA_SCH_CFG));
	seq_puts(seq, strprint);

#endif
	sprintf(strprint, "ports: %x %x %x %x %x %x\n", sysRegRead(PORT0_PKCOUNT), sysRegRead(PORT1_PKCOUNT), sysRegRead(PORT2_PKCOUNT), sysRegRead(PORT3_PKCOUNT), sysRegRead(PORT4_PKCOUNT), sysRegRead(PORT5_PKCOUNT));
	seq_puts(seq, strprint);

	return 0;
}

static int ra_snmp_seq_open(struct inode *inode, struct file *file)
{
	return single_open(file, ra_snmp_seq_show, NULL);
}

static const struct file_operations ra_snmp_seq_fops = {
	.owner	 = THIS_MODULE,
	.open	 = ra_snmp_seq_open,
	.read	 = seq_read,
	.llseek	 = seq_lseek,
	.release = single_release,
};
#endif


#if defined (CONFIG_GIGAPHY) || defined (CONFIG_100PHY) || defined (CONFIG_P5_MAC_TO_PHY_MODE)
#if defined (CONFIG_RALINK_RT6855) || defined(CONFIG_RALINK_RT63365) || \
    defined (CONFIG_RALINK_RT6352) || defined(CONFIG_RALINK_RT71100) 
void enable_auto_negotiate(int unused)
{
	u32 regValue;
	u32 addr = CONFIG_MAC_TO_GIGAPHY_MODE_ADDR;

	/* FIXME: we don't know how to deal with PHY end addr */
	regValue = sysRegRead(ESW_PHY_POLLING);
	regValue |= (1<<31);
	regValue &= ~(0x1f);
	regValue &= ~(0x1f<<8);
#if defined (CONFIG_RALINK_RT6352) || defined(CONFIG_RALINK_RT71100)
	regValue |= ((addr-1) << 0);//hardware limitation
#else
	regValue |= ((addr) << 0);// setup PHY address for auto polling (start Addr).
#endif
	regValue |= (addr << 8);// setup PHY address for auto polling (End Addr).

	sysRegWrite(ESW_PHY_POLLING, regValue);

#if defined (CONFIG_P4_MAC_TO_PHY_MODE)
	//FIXME: HW auto polling has bug
	*(unsigned long *)(RALINK_ETH_SW_BASE+0x3400) &= ~(0x1 << 15);
#endif
#if defined (CONFIG_P5_MAC_TO_PHY_MODE)
	*(unsigned long *)(RALINK_ETH_SW_BASE+0x3500) &= ~(0x1 << 15);
#endif
}
#else

void enable_auto_negotiate(int ge)
{
#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352)
        u32 regValue = sysRegRead(0xb01100C8);
#else
	u32 regValue;
	regValue = (ge == 2)? sysRegRead(MDIO_CFG2) : sysRegRead(MDIO_CFG);
#endif

        regValue &= 0xe0ff7fff;                 // clear auto polling related field:
                                                // (MD_PHY1ADDR & GP1_FRC_EN).
        regValue |= 0x20000000;                 // force to enable MDC/MDIO auto polling.

#if defined (CONFIG_GE2_RGMII_AN) || defined (CONFIG_GE2_MII_AN)
	if(ge==2) {
	    regValue |= (CONFIG_MAC_TO_GIGAPHY_MODE_ADDR2 << 24);               // setup PHY address for auto polling.
	}
#endif
#if defined (CONFIG_GE1_RGMII_AN) || defined (CONFIG_GE1_MII_AN) || defined (CONFIG_P5_MAC_TO_PHY_MODE)
	if(ge==1) {
	    regValue |= (CONFIG_MAC_TO_GIGAPHY_MODE_ADDR << 24);               // setup PHY address for auto polling.
	}
#endif

#if defined (CONFIG_RALINK_RT3052) || defined (CONFIG_RALINK_RT3352)
	sysRegWrite(0xb01100C8, regValue);
#else
	if (ge == 2)
		sysRegWrite(MDIO_CFG2, regValue);
	else
		sysRegWrite(MDIO_CFG, regValue);
#endif
}
#endif
#endif
void ra2880stop(END_DEVICE *ei_local)
{
	unsigned int regValue;
	printk("ra2880stop()...");

	regValue = sysRegRead(PDMA_GLO_CFG);
	regValue &= ~(TX_WB_DDONE | RX_DMA_EN | TX_DMA_EN);
	sysRegWrite(PDMA_GLO_CFG, regValue);
    	
	printk("Done\n");	
	// printk("Done0x%x...\n", readreg(PDMA_GLO_CFG));
}

void ei_irq_clear(void)
{
        sysRegWrite(FE_INT_STATUS, 0xFFFFFFFF);
}

void rt2880_gmac_hard_reset(void)
{
#if !defined (CONFIG_RALINK_RT63365)
	//FIXME
	sysRegWrite(RSTCTRL, RALINK_FE_RST);
	sysRegWrite(RSTCTRL, 0);
#endif
}

void ra2880EnableInterrupt()
{
	unsigned int regValue = sysRegRead(FE_INT_ENABLE);
	RAETH_PRINT("FE_INT_ENABLE -- : 0x%08x\n", regValue);
//	regValue |= (RX_DONE_INT0 | TX_DONE_INT0);
		
	sysRegWrite(FE_INT_ENABLE, regValue);
}

void ra2880MacAddressSet(unsigned char p[6])
{
        unsigned long regValue;

	regValue = (p[0] << 8) | (p[1]);
#if defined (CONFIG_RALINK_RT5350)
        sysRegWrite(SDM_MAC_ADRH, regValue);
	printk("MAC_ADRH -- : 0x%08x\n", sysRegRead(SDM_MAC_ADRH));
#elif defined (CONFIG_RALINK_RT6855) || defined(CONFIG_RALINK_RT63365)
        sysRegWrite(GDMA1_MAC_ADRH, regValue);
	printk("MAC_ADRH -- : 0x%08x\n", sysRegRead(GDMA1_MAC_ADRH));

	/* To keep the consistence between RT6855 and RT62806, GSW should keep the register. */
        sysRegWrite(SMACCR1, regValue);
	printk("SMACCR1 -- : 0x%08x\n", sysRegRead(SMACCR1));
#elif defined (CONFIG_RALINK_RT6352) || defined(CONFIG_RALINK_RT71100)
        sysRegWrite(SMACCR1, regValue);
	printk("SMACCR1 -- : 0x%08x\n", sysRegRead(SMACCR1));
#else
        sysRegWrite(GDMA1_MAC_ADRH, regValue);
	printk("MAC_ADRH -- : 0x%08x\n", sysRegRead(GDMA1_MAC_ADRH));
#endif

        regValue = (p[2] << 24) | (p[3] <<16) | (p[4] << 8) | p[5];
#if defined (CONFIG_RALINK_RT5350)
        sysRegWrite(SDM_MAC_ADRL, regValue);
	printk("MAC_ADRL -- : 0x%08x\n", sysRegRead(SDM_MAC_ADRL));	    
#elif defined (CONFIG_RALINK_RT6855) || defined(CONFIG_RALINK_RT63365)
        sysRegWrite(GDMA1_MAC_ADRL, regValue);
	printk("MAC_ADRL -- : 0x%08x\n", sysRegRead(GDMA1_MAC_ADRL));	    

	/* To keep the consistence between RT6855 and RT62806, GSW should keep the register. */
        sysRegWrite(SMACCR0, regValue);
	printk("SMACCR0 -- : 0x%08x\n", sysRegRead(SMACCR0));
#elif defined (CONFIG_RALINK_RT6352) || defined(CONFIG_RALINK_RT71100)
        sysRegWrite(SMACCR0, regValue);
	printk("SMACCR0 -- : 0x%08x\n", sysRegRead(SMACCR0));
#else
        sysRegWrite(GDMA1_MAC_ADRL, regValue);
	printk("MAC_ADRL -- : 0x%08x\n", sysRegRead(GDMA1_MAC_ADRL));	    
#endif

        return;
}

#ifdef CONFIG_PSEUDO_SUPPORT
void ra2880Mac2AddressSet(unsigned char p[6])
{
        unsigned long regValue;

	regValue = (p[0] << 8) | (p[1]);
        sysRegWrite(GDMA2_MAC_ADRH, regValue);

        regValue = (p[2] << 24) | (p[3] <<16) | (p[4] << 8) | p[5];
        sysRegWrite(GDMA2_MAC_ADRL, regValue);

	printk("GDMA2_MAC_ADRH -- : 0x%08x\n", sysRegRead(GDMA2_MAC_ADRH));
	printk("GDMA2_MAC_ADRL -- : 0x%08x\n", sysRegRead(GDMA2_MAC_ADRL));	    
        return;
}
#endif

/**
 * hard_init - Called by raeth_probe to inititialize network device
 * @dev: device pointer
 *
 * ethdev_init initilize dev->priv and set to END_DEVICE structure
 *
 */
void ethtool_init(struct net_device *dev)
{
#if defined (CONFIG_ETHTOOL) && defined (CONFIG_RAETH_ROUTER)
	END_DEVICE *ei_local = netdev_priv(dev);

	// init mii structure
	ei_local->mii_info.dev = dev;
	ei_local->mii_info.mdio_read = mdio_read;
	ei_local->mii_info.mdio_write = mdio_write;
	ei_local->mii_info.phy_id_mask = 0x1f;
	ei_local->mii_info.reg_num_mask = 0x1f;
	ei_local->mii_info.supports_gmii = mii_check_gmii_support(&ei_local->mii_info);
	// TODO:   phy_id: 0~4
	ei_local->mii_info.phy_id = 1;
#endif
	return;
}

#if defined(CONFIG_RAETH_QOS)
/*
 *	Routine Name : get_idx(mode, index)
 *	Description: calculate ring usage for tx/rx rings
 *	Mode 1 : Tx Ring 
 *	Mode 2 : Rx Ring
 */
int get_ring_usage(int mode, int i)
{
	unsigned long tx_ctx_idx, tx_dtx_idx, tx_usage;
	unsigned long rx_calc_idx, rx_drx_idx, rx_usage;

	struct PDMA_rxdesc* rxring;
	struct PDMA_txdesc* txring;

	END_DEVICE *ei_local = netdev_priv(dev_raether);


	if (mode == 2 ) {
		/* cpu point to the next descriptor of rx dma ring */
	        rx_calc_idx = *(unsigned long*)RX_CALC_IDX0;
	        rx_drx_idx = *(unsigned long*)RX_DRX_IDX0;
		rxring = (struct PDMA_rxdesc*)RX_BASE_PTR0;
		
		rx_usage = (rx_drx_idx - rx_calc_idx -1 + NUM_RX_DESC) % NUM_RX_DESC;
		if ( rx_calc_idx == rx_drx_idx ) {
		    if ( rxring[rx_drx_idx].rxd_info2.DDONE_bit == 1)
			tx_usage = NUM_RX_DESC;
		    else
			tx_usage = 0;
		}
		return rx_usage;
	}

	
	switch (i) {
		case 0:
				tx_ctx_idx = *(unsigned long*)TX_CTX_IDX0;
				tx_dtx_idx = *(unsigned long*)TX_DTX_IDX0;
				txring = ei_local->tx_ring0;
				break;
		case 1:
				tx_ctx_idx = *(unsigned long*)TX_CTX_IDX1;
				tx_dtx_idx = *(unsigned long*)TX_DTX_IDX1;
				txring = ei_local->tx_ring1;
				break;
		case 2:
				tx_ctx_idx = *(unsigned long*)TX_CTX_IDX2;
				tx_dtx_idx = *(unsigned long*)TX_DTX_IDX2;
				txring = ei_local->tx_ring2;
				break;
		case 3:
				tx_ctx_idx = *(unsigned long*)TX_CTX_IDX3;
				tx_dtx_idx = *(unsigned long*)TX_DTX_IDX3;
				txring = ei_local->tx_ring3;
				break;
		default:
			printk("get_tx_idx failed %d %d\n", mode, i);
			return 0;
	};

	tx_usage = (tx_ctx_idx - tx_dtx_idx + NUM_TX_DESC) % NUM_TX_DESC;
	if ( tx_ctx_idx == tx_dtx_idx ) {
		if ( txring[tx_ctx_idx].txd_info2.DDONE_bit == 1)
			tx_usage = 0;
		else
			tx_usage = NUM_TX_DESC;
	}
	return tx_usage;

}

void dump_qos()
{
	int usage;
	int i;

	printk("\n-----Raeth QOS -----\n\n");

	for ( i = 0; i < 4; i++)  {
		usage = get_ring_usage(1,i);
		printk("Tx Ring%d Usage : %d/%d\n", i, usage, NUM_TX_DESC);
	}

	usage = get_ring_usage(2,0);
	printk("RX Usage : %d/%d\n\n", usage, NUM_RX_DESC);
#if defined  (CONFIG_RALINK_RT6352)
	printk("PSE_FQFC_CFG(0x%08x)  : 0x%08x\n", PSE_FQFC_CFG, sysRegRead(PSE_FQFC_CFG));
	printk("PSE_IQ_CFG(0x%08x)  : 0x%08x\n", PSE_IQ_CFG, sysRegRead(PSE_IQ_CFG));
	printk("PSE_QUE_STA(0x%08x)  : 0x%08x\n", PSE_QUE_STA, sysRegRead(PSE_QUE_STA));
#elif defined (CONFIG_RALINK_RT5350)

#else
	printk("GDMA1_FC_CFG(0x%08x)  : 0x%08x\n", GDMA1_FC_CFG, sysRegRead(GDMA1_FC_CFG));
	printk("GDMA2_FC_CFG(0x%08x)  : 0x%08x\n", GDMA2_FC_CFG, sysRegRead(GDMA2_FC_CFG));
	printk("PDMA_FC_CFG(0x%08x)  : 0x%08x\n", PDMA_FC_CFG, sysRegRead(PDMA_FC_CFG));
	printk("PSE_FQ_CFG(0x%08x)  : 0x%08x\n", PSE_FQ_CFG, sysRegRead(PSE_FQ_CFG));
#endif
	printk("\n\nTX_CTX_IDX0    : 0x%08x\n", sysRegRead(TX_CTX_IDX0));	
	printk("TX_DTX_IDX0    : 0x%08x\n", sysRegRead(TX_DTX_IDX0));
	printk("TX_CTX_IDX1    : 0x%08x\n", sysRegRead(TX_CTX_IDX1));	
	printk("TX_DTX_IDX1    : 0x%08x\n", sysRegRead(TX_DTX_IDX1));
	printk("TX_CTX_IDX2    : 0x%08x\n", sysRegRead(TX_CTX_IDX2));	
	printk("TX_DTX_IDX2    : 0x%08x\n", sysRegRead(TX_DTX_IDX2));
	printk("TX_CTX_IDX3    : 0x%08x\n", sysRegRead(TX_CTX_IDX3));
	printk("TX_DTX_IDX3    : 0x%08x\n", sysRegRead(TX_DTX_IDX3));
	printk("RX_CALC_IDX0   : 0x%08x\n", sysRegRead(RX_CALC_IDX0));
	printk("RX_DRX_IDX0    : 0x%08x\n", sysRegRead(RX_DRX_IDX0));

	printk("\n------------------------------\n\n");
}
#endif

void dump_reg()
{
	printk("\n\nFE_INT_ENABLE  : 0x%08x\n", sysRegRead(FE_INT_ENABLE));
	printk("DLY_INT_CFG	: 0x%08x\n", sysRegRead(DLY_INT_CFG));
	printk("TX_BASE_PTR0   : 0x%08x\n", sysRegRead(TX_BASE_PTR0));	
	printk("TX_CTX_IDX0    : 0x%08x\n", sysRegRead(TX_CTX_IDX0));	
	printk("TX_DTX_IDX0    : 0x%08x\n", sysRegRead(TX_DTX_IDX0));
	printk("TX_BASE_PTR1(0x%08x)   : 0x%08x\n", TX_BASE_PTR1, sysRegRead(TX_BASE_PTR1));	
	printk("TX_CTX_IDX1(0x%08x)    : 0x%08x\n", TX_CTX_IDX1, sysRegRead(TX_CTX_IDX1));
	printk("TX_DTX_IDX1(0x%08x)    : 0x%08x\n", TX_DTX_IDX1, sysRegRead(TX_DTX_IDX1));
	printk("TX_BASE_PTR2(0x%08x)   : 0x%08x\n", TX_BASE_PTR2, sysRegRead(TX_BASE_PTR2));	
	printk("TX_CTX_IDX2(0x%08x)    : 0x%08x\n", TX_CTX_IDX2, sysRegRead(TX_CTX_IDX2));
	printk("TX_DTX_IDX2(0x%08x)    : 0x%08x\n", TX_DTX_IDX2, sysRegRead(TX_DTX_IDX2));
	printk("TX_BASE_PTR3(0x%08x)   : 0x%08x\n", TX_BASE_PTR3, sysRegRead(TX_BASE_PTR3));	
	printk("TX_CTX_IDX3(0x%08x)    : 0x%08x\n", TX_CTX_IDX3, sysRegRead(TX_CTX_IDX3));
	printk("TX_DTX_IDX3(0x%08x)    : 0x%08x\n", TX_DTX_IDX3, sysRegRead(TX_DTX_IDX3));

	printk("RX_BASE_PTR0   : 0x%08x\n", sysRegRead(RX_BASE_PTR0));	
	printk("RX_MAX_CNT0    : 0x%08x\n", sysRegRead(RX_MAX_CNT0));	
	printk("RX_CALC_IDX0   : 0x%08x\n", sysRegRead(RX_CALC_IDX0));
	printk("RX_DRX_IDX0    : 0x%08x\n", sysRegRead(RX_DRX_IDX0));
	
#if defined (CONFIG_ETHTOOL) && defined (CONFIG_RAETH_ROUTER)
	printk("The current PHY address selected by ethtool is %d\n", get_current_phy_address());
#endif

#if defined (CONFIG_RALINK_RT2883) || defined(CONFIG_RALINK_RT3883)
	printk("GDMA_RX_FCCNT1(0x%08x)     : 0x%08x\n\n", GDMA_RX_FCCNT1, sysRegRead(GDMA_RX_FCCNT1));	
#endif
}

void dump_cp0(void)
{
	printk("CP0 Register dump --\n");
	printk("CP0_INDEX\t: 0x%08x\n", read_32bit_cp0_register(CP0_INDEX));
	printk("CP0_RANDOM\t: 0x%08x\n", read_32bit_cp0_register(CP0_RANDOM));
	printk("CP0_ENTRYLO0\t: 0x%08x\n", read_32bit_cp0_register(CP0_ENTRYLO0));
	printk("CP0_ENTRYLO1\t: 0x%08x\n", read_32bit_cp0_register(CP0_ENTRYLO1));
	printk("CP0_CONF\t: 0x%08x\n", read_32bit_cp0_register(CP0_CONF));
	printk("CP0_CONTEXT\t: 0x%08x\n", read_32bit_cp0_register(CP0_CONTEXT));
	printk("CP0_PAGEMASK\t: 0x%08x\n", read_32bit_cp0_register(CP0_PAGEMASK));
	printk("CP0_WIRED\t: 0x%08x\n", read_32bit_cp0_register(CP0_WIRED));
	printk("CP0_INFO\t: 0x%08x\n", read_32bit_cp0_register(CP0_INFO));
	printk("CP0_BADVADDR\t: 0x%08x\n", read_32bit_cp0_register(CP0_BADVADDR));
	printk("CP0_COUNT\t: 0x%08x\n", read_32bit_cp0_register(CP0_COUNT));
	printk("CP0_ENTRYHI\t: 0x%08x\n", read_32bit_cp0_register(CP0_ENTRYHI));
	printk("CP0_COMPARE\t: 0x%08x\n", read_32bit_cp0_register(CP0_COMPARE));
	printk("CP0_STATUS\t: 0x%08x\n", read_32bit_cp0_register(CP0_STATUS));
	printk("CP0_CAUSE\t: 0x%08x\n", read_32bit_cp0_register(CP0_CAUSE));
	printk("CP0_EPC\t: 0x%08x\n", read_32bit_cp0_register(CP0_EPC));
	printk("CP0_PRID\t: 0x%08x\n", read_32bit_cp0_register(CP0_PRID));
	printk("CP0_CONFIG\t: 0x%08x\n", read_32bit_cp0_register(CP0_CONFIG));
	printk("CP0_LLADDR\t: 0x%08x\n", read_32bit_cp0_register(CP0_LLADDR));
	printk("CP0_WATCHLO\t: 0x%08x\n", read_32bit_cp0_register(CP0_WATCHLO));
	printk("CP0_WATCHHI\t: 0x%08x\n", read_32bit_cp0_register(CP0_WATCHHI));
	printk("CP0_XCONTEXT\t: 0x%08x\n", read_32bit_cp0_register(CP0_XCONTEXT));
	printk("CP0_FRAMEMASK\t: 0x%08x\n", read_32bit_cp0_register(CP0_FRAMEMASK));
	printk("CP0_DIAGNOSTIC\t: 0x%08x\n", read_32bit_cp0_register(CP0_DIAGNOSTIC));
	printk("CP0_DEBUG\t: 0x%08x\n", read_32bit_cp0_register(CP0_DEBUG));
	printk("CP0_DEPC\t: 0x%08x\n", read_32bit_cp0_register(CP0_DEPC));
	printk("CP0_PERFORMANCE\t: 0x%08x\n", read_32bit_cp0_register(CP0_PERFORMANCE));
	printk("CP0_ECC\t: 0x%08x\n", read_32bit_cp0_register(CP0_ECC));
	printk("CP0_CACHEERR\t: 0x%08x\n", read_32bit_cp0_register(CP0_CACHEERR));
	printk("CP0_TAGLO\t: 0x%08x\n", read_32bit_cp0_register(CP0_TAGLO));
	printk("CP0_TAGHI\t: 0x%08x\n", read_32bit_cp0_register(CP0_TAGHI));
	printk("CP0_ERROREPC\t: 0x%08x\n", read_32bit_cp0_register(CP0_ERROREPC));
	printk("CP0_DESAVE\t: 0x%08x\n\n", read_32bit_cp0_register(CP0_DESAVE));
}

struct proc_dir_entry *procRegDir;
static struct proc_dir_entry *procGmac, *procSysCP0, *procTxRing, *procRxRing, *procSkbFree;
#if defined(CONFIG_USER_SNMPD)
static struct proc_dir_entry *procRaSnmp;
#endif

int RegReadMain(void)
{
	dump_reg();
	return 0;
}

int SkbFreeRead(void)
{
	END_DEVICE *ei_local = netdev_priv(dev_raether);
	int i = 0;

	for (i=0; i < NUM_TX_DESC; i++) {
		printk("%d: %08x\n",i,  *(int *)&ei_local->skb_free[i]);
        }
	return 0;
}
int TxRingRead(void)
{
	END_DEVICE *ei_local = netdev_priv(dev_raether);
	int i = 0;

	for (i=0; i < NUM_TX_DESC; i++) {
		printk("%d: %08x %08x %08x %08x\n",i,  *(int *)&ei_local->tx_ring0[i].txd_info1,
				*(int *)&ei_local->tx_ring0[i].txd_info2, *(int *)&ei_local->tx_ring0[i].txd_info3,
				*(int *)&ei_local->tx_ring0[i].txd_info4);
        }
	return 0;
}

int RxRingRead(void)
{
	END_DEVICE *ei_local = netdev_priv(dev_raether);
	int i = 0;

	for (i=0; i < NUM_RX_DESC; i++) {
		printk("%d: %08x %08x %08x %08x\n",i,  *(int *)&ei_local->rx_ring0[i].rxd_info1,
				*(int *)&ei_local->rx_ring0[i].rxd_info2, *(int *)&ei_local->rx_ring0[i].rxd_info3,
				*(int *)&ei_local->rx_ring0[i].rxd_info4);
        }
	return 0;
}

int CP0RegRead(void)
{
	dump_cp0();
	return 0;
}

#if defined(CONFIG_RAETH_QOS)
static struct proc_dir_entry *procRaQOS, *procRaFeIntr, *procRaEswIntr;
extern uint32_t num_of_rxdone_intr;
extern uint32_t num_of_esw_intr;

int RaQOSRegRead(void)
{
	dump_qos();
	return 0;
}
#endif

#if defined(CONFIG_RT_3052_ESW)
static struct proc_dir_entry *procEswCnt;

int EswCntRead(void)
{
	printk("\n		  <<CPU>>			 \n");
	printk("		    |				 \n");
#if defined (CONFIG_RALINK_RT5350)
	printk("+-----------------------------------------------+\n");
	printk("|		  <<PDMA>>		        |\n");
	printk("+-----------------------------------------------+\n");
#else
	printk("+-----------------------------------------------+\n");
	printk("|		  <<PSE>>		        |\n");
	printk("+-----------------------------------------------+\n");
	printk("		   |				 \n");
	printk("+-----------------------------------------------+\n");
	printk("|		  <<GDMA>>		        |\n");
#if defined (CONFIG_RALINK_RT6352) || defined (CONFIG_RALINK_RT71100)

	printk("| GDMA1_TX_GPCNT  : %010d (Tx Good Pkts)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1304));	
	printk("| GDMA1_RX_GPCNT  : %010d (Rx Good Pkts)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1324));	
	printk("|						|\n");
	printk("| GDMA1_TX_SKIPCNT: %010d (skip)		|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1308));	
	printk("| GDMA1_TX_COLCNT : %010d (collision)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x130c));	
	printk("| GDMA1_RX_OERCNT : %010d (overflow)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1328));	
	printk("| GDMA1_RX_FERCNT : %010d (FCS error)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x132c));	
	printk("| GDMA1_RX_SERCNT : %010d (too short)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1330));	
	printk("| GDMA1_RX_LERCNT : %010d (too long)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1334));	
	printk("| GDMA1_RX_CERCNT : %010d (l3/l4 checksum) |\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1338));	
	printk("| GDMA1_RX_FCCNT  : %010d (flow control)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x133c));	

	printk("|						|\n");
	printk("| GDMA2_TX_GPCNT  : %010d (Tx Good Pkts)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1344));	
	printk("| GDMA2_RX_GPCNT  : %010d (Rx Good Pkts)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1364));	
	printk("|						|\n");
	printk("| GDMA2_TX_SKIPCNT: %010d (skip)		|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1348));	
	printk("| GDMA2_TX_COLCNT : %010d (collision)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x134c));	
	printk("| GDMA2_RX_OERCNT : %010d (overflow)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1368));	
	printk("| GDMA2_RX_FERCNT : %010d (FCS error)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x136c));	
	printk("| GDMA2_RX_SERCNT : %010d (too short)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1370));	
	printk("| GDMA2_RX_LERCNT : %010d (too long)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1374));	
	printk("| GDMA2_RX_CERCNT : %010d (l3/l4 checksum) |\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x1378));	
	printk("| GDMA2_RX_FCCNT  : %010d (flow control)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x137c));	
#else
	printk("| GDMA_TX_GPCNT1  : %010d (Tx Good Pkts)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x704));	
	printk("| GDMA_RX_GPCNT1  : %010d (Rx Good Pkts)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x724));	
	printk("|						|\n");
	printk("| GDMA_TX_SKIPCNT1: %010d (skip)		|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x708));	
	printk("| GDMA_TX_COLCNT1 : %010d (collision)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x70c));	
	printk("| GDMA_RX_OERCNT1 : %010d (overflow)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x728));	
	printk("| GDMA_RX_FERCNT1 : %010d (FCS error)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x72c));	
	printk("| GDMA_RX_SERCNT1 : %010d (too short)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x730));	
	printk("| GDMA_RX_LERCNT1 : %010d (too long)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x734));	
	printk("| GDMA_RX_CERCNT1 : %010d (l3/l4 checksum)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x738));	
	printk("| GDMA_RX_FCCNT1  : %010d (flow control)	|\n", sysRegRead(RALINK_FRAME_ENGINE_BASE+0x73c));	

#endif
	printk("+-----------------------------------------------+\n");
#endif

#if defined (CONFIG_RALINK_RT6855) || defined(CONFIG_RALINK_RT63365) || \
    defined (CONFIG_RALINK_RT6352) || defined(CONFIG_RALINK_RT71100)

	printk("                      ^                          \n");
	printk("                      | Port6 Rx:%08d Good Pkt   \n", sysRegRead(RALINK_ETH_SW_BASE+0x4620)&0xFFFF);
	printk("                      | Port6 Rx:%08d Bad Pkt    \n", sysRegRead(RALINK_ETH_SW_BASE+0x4620)>>16);
	printk("                      | Port6 Tx:%08d Good Pkt   \n", sysRegRead(RALINK_ETH_SW_BASE+0x4610)&0xFFFF);
	printk("                      | Port6 Tx:%08d Bad Pkt    \n", sysRegRead(RALINK_ETH_SW_BASE+0x4610)>>16);
#if defined (CONFIG_RALINK_RT6352)
	printk("                      | Port7 Rx:%08d Good Pkt   \n", sysRegRead(RALINK_ETH_SW_BASE+0x4720)&0xFFFF);
	printk("                      | Port7 Rx:%08d Bad Pkt    \n", sysRegRead(RALINK_ETH_SW_BASE+0x4720)>>16);
	printk("                      | Port7 Tx:%08d Good Pkt   \n", sysRegRead(RALINK_ETH_SW_BASE+0x4710)&0xFFFF);
	printk("                      | Port7 Tx:%08d Bad Pkt    \n", sysRegRead(RALINK_ETH_SW_BASE+0x4710)>>16);
#endif
	printk("+---------------------v-------------------------+\n");
	printk("|		      P6		        |\n");
	printk("|        <<10/100/1000 Embedded Switch>>        |\n");
	printk("|     P0    P1    P2     P3     P4     P5       |\n");
	printk("+-----------------------------------------------+\n");
	printk("       |     |     |     |       |      |        \n");
#else
	printk("                      ^                          \n");
	printk("                      | Port6 Rx:%08d Good Pkt   \n", sysRegRead(RALINK_ETH_SW_BASE+0xE0)&0xFFFF);
	printk("                      | Port6 Tx:%08d Good Pkt   \n", sysRegRead(RALINK_ETH_SW_BASE+0xE0)>>16);
	printk("+---------------------v-------------------------+\n");
	printk("|		      P6		        |\n");
	printk("|  	     <<10/100 Embedded Switch>>	        |\n");
	printk("|     P0    P1    P2     P3     P4     P5       |\n");
	printk("+-----------------------------------------------+\n");
	printk("       |     |     |     |       |      |        \n");
#endif

#if defined (CONFIG_RALINK_RT6855) || defined(CONFIG_RALINK_RT63365) || \
    defined (CONFIG_RALINK_RT6352) || defined(CONFIG_RALINK_RT71100)
	printk("Port0 Good RX=%08d Tx=%08d (Bad Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0x4020)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4010)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4020)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x4010)>>16);

	printk("Port1 Good RX=%08d Tx=%08d (Bad Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0x4120)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4110)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4120)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x4110)>>16);

	printk("Port2 Good RX=%08d Tx=%08d (Bad Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0x4220)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4210)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4220)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x4210)>>16);

	printk("Port3 Good RX=%08d Tx=%08d (Bad Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0x4320)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4310)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4320)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x4310)>>16);

	printk("Port4 Good RX=%08d Tx=%08d (Bad Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0x4420)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4410)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4420)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x4410)>>16);

	printk("Port5 Good RX=%08d Tx=%08d (Bad Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0x4520)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4510)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x4520)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x4510)>>16);

#elif defined (CONFIG_RALINK_RT5350)
	printk("Port0 Good Pkt Cnt: RX=%08d Tx=%08d (Bad Pkt Cnt: Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xE8)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x150)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xE8)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x150)>>16);

	printk("Port1 Good Pkt Cnt: RX=%08d Tx=%08d (Bad Pkt Cnt: Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xEC)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x154)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xEC)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x154)>>16);

	printk("Port2 Good Pkt Cnt: RX=%08d Tx=%08d (Bad Pkt Cnt: Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xF0)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x158)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xF0)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x158)>>16);

	printk("Port3 Good Pkt Cnt: RX=%08d Tx=%08d (Bad Pkt Cnt: Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xF4)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x15C)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xF4)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x15c)>>16);

	printk("Port4 Good Pkt Cnt: RX=%08d Tx=%08d (Bad Pkt Cnt: Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xF8)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x160)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xF8)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x160)>>16);

	printk("Port5 Good Pkt Cnt: RX=%08d Tx=%08d (Bad Pkt Cnt: Rx=%08d Tx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xFC)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0x164)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xFC)>>16, sysRegRead(RALINK_ETH_SW_BASE+0x164)>>16);
#else /* RT305x, RT3352 */
	printk("Port0: Good Pkt Cnt: RX=%08d (Bad Pkt Cnt: Rx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xE8)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xE8)>>16);
	printk("Port1: Good Pkt Cnt: RX=%08d (Bad Pkt Cnt: Rx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xEC)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xEC)>>16);
	printk("Port2: Good Pkt Cnt: RX=%08d (Bad Pkt Cnt: Rx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xF0)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xF0)>>16);
	printk("Port3: Good Pkt Cnt: RX=%08d (Bad Pkt Cnt: Rx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xF4)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xF4)>>16);
	printk("Port4: Good Pkt Cnt: RX=%08d (Bad Pkt Cnt: Rx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xF8)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xF8)>>16);
	printk("Port5: Good Pkt Cnt: RX=%08d (Bad Pkt Cnt: Rx=%08d)\n", sysRegRead(RALINK_ETH_SW_BASE+0xFC)&0xFFFF,sysRegRead(RALINK_ETH_SW_BASE+0xFC)>>16);
#endif
	printk("\n");

	return 0;
}

#endif

#if defined (CONFIG_ETHTOOL) && defined (CONFIG_RAETH_ROUTER)
/*
 * proc write procedure
 */
static int change_phyid(struct file *file, const char *buffer, unsigned long count, void *data)
{
	char buf[32];
	struct net_device *cur_dev_p;
	END_DEVICE *ei_local;
	char if_name[64];
	unsigned int phy_id;

	if (count > 32)
		count = 32;
	memset(buf, 0, 32);
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	/* determine interface name */
    strcpy(if_name, DEV_NAME);	/* "eth2" by default */
    if(isalpha(buf[0]))
		sscanf(buf, "%s %d", if_name, &phy_id);
	else
		phy_id = simple_strtol(buf, 0, 10);

	for(cur_dev_p=dev_base; cur_dev_p!=NULL; cur_dev_p=cur_dev_p->next){
		if (strncmp(cur_dev_p->name, if_name, 4) == 0)
			break;
	}
	if (cur_dev_p == NULL)
		return -EFAULT;

#ifdef CONFIG_PSEUDO_SUPPORT
	/* This may be wrong when more than 2 gmacs */
	if(!strcmp(cur_dev_p->name, DEV_NAME)){
		ei_local = cur_dev_p->priv;
		ei_local->mii_info.phy_id = (unsigned char)phy_id;
	}else{
		PSEUDO_ADAPTER *pPseudoAd;
		pPseudoAd = cur_dev_p->priv;
		pPseudoAd->mii_info.phy_id = (unsigned char)phy_id;
	}
#else
	ei_local = cur_dev_p->priv;
	ei_local->mii_info.phy_id = (unsigned char)phy_id;
#endif
	return count;
}
#endif

int debug_proc_init(void)
{
    if (procRegDir == NULL)
	procRegDir = proc_mkdir(PROCREG_DIR, NULL);
   
    if ((procGmac = create_proc_entry(PROCREG_GMAC, 0, procRegDir))){
	 procGmac->read_proc = (read_proc_t*)&RegReadMain;
#if defined (CONFIG_ETHTOOL) && defined (CONFIG_RAETH_ROUTER)
	 procGmac->write_proc = (write_proc_t*)&change_phyid;
#endif
	}

    if ((procSkbFree = create_proc_entry(PROCREG_SKBFREE, 0, procRegDir)))
	 procSkbFree->read_proc = (read_proc_t*)&SkbFreeRead;

    if ((procTxRing = create_proc_entry(PROCREG_TXRING, 0, procRegDir)))
	 procTxRing->read_proc = (read_proc_t*)&TxRingRead;
    
    if ((procRxRing = create_proc_entry(PROCREG_RXRING, 0, procRegDir)))
	 procRxRing->read_proc = (read_proc_t*)&RxRingRead;

    if ((procSysCP0 = create_proc_entry(PROCREG_CP0, 0, procRegDir)))
	 procSysCP0->read_proc = (read_proc_t*)&CP0RegRead;
     
#if defined(CONFIG_RAETH_QOS)
    if ((procRaQOS = create_proc_entry(PROCREG_RAQOS, 0, procRegDir)))
	 procRaQOS->read_proc = (read_proc_t*)&RaQOSRegRead;
#endif

#if defined(CONFIG_USER_SNMPD)
    procRaSnmp = create_proc_entry(PROCREG_SNMP, S_IRUGO, procRegDir);
    if (procRaSnmp == NULL)
    	printk(KERN_ALERT "raeth: snmp proc create failed!!!");
    else
    	procRaSnmp->proc_fops = &ra_snmp_seq_fops;
#endif
   
#if defined (CONFIG_RT_3052_ESW) 
    if ((procEswCnt = create_proc_entry( PROCREG_ESW_CNT, 0, procRegDir))){
	 procEswCnt->read_proc = (read_proc_t*)&EswCntRead;
    }
#endif

    printk(KERN_ALERT "PROC INIT OK!\n");
    return 0;
}

void debug_proc_exit(void)
{

    if (procSysCP0)
    	remove_proc_entry(PROCREG_CP0, procRegDir);

    if (procGmac)
    	remove_proc_entry(PROCREG_GMAC, procRegDir);
    
    if (procSkbFree)
    	remove_proc_entry(PROCREG_SKBFREE, procRegDir);

    if (procTxRing)
    	remove_proc_entry(PROCREG_TXRING, procRegDir);
    
    if (procRxRing)
    	remove_proc_entry(PROCREG_RXRING, procRegDir);
    
#if defined(CONFIG_RAETH_QOS)
    if (procRaQOS)
    	remove_proc_entry(PROCREG_RAQOS, procRegDir);
    if (procRaFeIntr)
    	remove_proc_entry(PROCREG_RXDONE_INTR, procRegDir);
    if (procRaEswIntr)
    	remove_proc_entry(PROCREG_ESW_INTR, procRegDir);
#endif

#if defined(CONFIG_USER_SNMPD)
    if (procRaSnmp)
	remove_proc_entry(PROCREG_SNMP, procRegDir);
#endif

#if defined (CONFIG_RT_3052_ESW) 
    if (procEswCnt)
    	remove_proc_entry(PROCREG_ESW_CNT, procRegDir);
#endif
    //if (procRegDir)
   	//remove_proc_entry(PROCREG_DIR, 0);
	
    printk(KERN_ALERT "proc exit\n");
}
EXPORT_SYMBOL(procRegDir);
