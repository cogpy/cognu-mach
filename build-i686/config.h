/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* APIC support */
/* #undef APIC */

/* AT386 */
#define AT386 1

/* ATX86_64 */
/* #undef ATX86_64 */

/* BOOTSTRAP_SYMBOLS */
#define BOOTSTRAP_SYMBOLS 0

/* CMU */
#define CMU 1

/* option 3c515: Linux device driver for Ethernet controller 3Com 515 ISA Fast
   EtherLink */
/* #undef CONFIG_3C515 */

/* option 3c574_cs: Linux device driver for 3Com 3c574 ``RoadRunner'' PCMCIA
   Ethernet; on ix86-at enabled by default */
#define CONFIG_3C574_CS 1

/* option 3c589_cs: Linux device driver for 3Com 3c589 PCMCIA Ethernet card;
   on ix86-at enabled by default */
#define CONFIG_3C589_CS 1

/* option ac3200: Linux device driver for Ethernet controller Ansel
   Communications EISA 3200; on ix86-at enabled by default */
#define CONFIG_AC3200 1

/* option apricot: Linux device driver for Ethernet controller Apricot XEN-II
   on board ethernet; on ix86-at enabled by default */
#define CONFIG_APRICOT 1

/* option at1700: Linux device driver for Ethernet controller AT1700 (Fujitsu
   86965); on ix86-at enabled by default */
#define CONFIG_AT1700 1

/* option atp: Linux device driver for Ethernet controller AT-LAN-TEC/RealTek
   pocket adaptor */
/* #undef CONFIG_ATP */

/* option axnet_cs: Linux device driver for Asix AX88190-based PCMCIA Ethernet
   adapters; on ix86-at enabled by default */
#define CONFIG_AXNET_CS 1

/* option floppy: Linux device driver for PC floppy; on ix86-at enabled by
   default and for qemu */
#define CONFIG_BLK_DEV_FD 1

/* Force DMA on IDE block devices */
/* #undef CONFIG_BLK_DEV_FORCE_DMA */

/* option ide: Linux device driver for IDE disk controllers; on ix86-at
   enabled by default and for qemu */
#define CONFIG_BLK_DEV_IDE 1

/* CONFIG_BLOCK */
#define CONFIG_BLOCK 1

/* option de4x5: Linux device driver for Ethernet controller DE4x5 (de4x5,
   de425, de434, de435, de450, de500); on ix86-at enabled by default */
#define CONFIG_DE4X5 1

/* option de600: Linux device driver for Ethernet controller D-Link DE-600 */
/* #undef CONFIG_DE600 */

/* option de620: Linux device driver for Ethernet controller D-Link DE-620 */
/* #undef CONFIG_DE620 */

/* option tulip: Linux device driver for Ethernet controller DECchip Tulip
   (dc21x4x) PCI (elcp, tulip); on ix86-at enabled by default */
#define CONFIG_DEC_ELCP 1

/* option depca: Linux device driver for Ethernet controller DEPCA (de100,
   de101, de200, de201, de202, de210, de422); on ix86-at enabled by default */
#define CONFIG_DEPCA 1

/* option e2100: Linux device driver for Ethernet controller Cabletron E21xx;
   on ix86-at enabled by default */
#define CONFIG_E2100 1

/* option eexpress: Linux device driver for Ethernet controller EtherExpress
   16; on ix86-at enabled by default */
#define CONFIG_EEXPRESS 1

/* option eepro: Linux device driver for Ethernet controller EtherExpressPro;
   on ix86-at enabled by default */
#define CONFIG_EEXPRESS_PRO 1

/* option eepro100: Linux device driver for Ethernet controller Intel
   EtherExpressPro PCI 10+/100B/100+; on ix86-at enabled by default */
#define CONFIG_EEXPRESS_PRO100B 1

/* option 3c501: Linux device driver for Ethernet controller 3COM 501 (3c501)
   / Etherlink I; on ix86-at enabled by default */
#define CONFIG_EL1 1

/* option 3c507: Linux device driver for Ethernet controller 3Com 507 (3c507,
   el16); on ix86-at enabled by default */
#define CONFIG_EL16 1

/* option 3c503: Linux device driver for Ethernet controller 3Com 503 (3c503)
   / Etherlink II; on ix86-at enabled by default */
#define CONFIG_EL2 1

/* option 3c509: Linux device driver for Ethernet controller 3Com 509/579
   (3c509, 3c579) / Etherlink III; on ix86-at enabled by default */
#define CONFIG_EL3 1

/* option 3c505: Linux device driver for Ethernet controller 3Com 505 (3c505,
   elplus); on ix86-at enabled by default */
#define CONFIG_ELPLUS 1

/* option epic100: Linux device driver for Ethernet controller SMC 83c170/175
   EPIC/100 (epic, epic100) / EtherPower II; on ix86-at enabled by default */
#define CONFIG_EPIC 1

/* option eth16i: Linux device driver for Ethernet controller ICL EtherTeam
   16i/32 (eth16i, eth32); on ix86-at enabled by default */
#define CONFIG_ETH16I 1

/* option ewrk3: Linux device driver for Ethernet controller EtherWORKS 3
   (ewrk3, de203, de204, de205); on ix86-at enabled by default */
#define CONFIG_EWRK3 1

/* option fmv18x: Linux device driver for Ethernet controller
   FMV-181/182/183/184; on ix86-at enabled by default */
#define CONFIG_FMV18X 1

/* option fmvj18x_cs: Linux device driver for fmvj18x chipset based PCMCIA
   Ethernet cards; on ix86-at enabled by default */
#define CONFIG_FMVJ18X_CS 1

/* option hamachi: Linux device driver for Ethernet controller Packet Engines
   "Hamachi" GNIC-2 Gigabit Ethernet; on ix86-at enabled by default */
#define CONFIG_HAMACHI 1

/* option hp100: Linux device driver for Ethernet controller HP 10/100VG PCLAN
   (ISA, EISA, PCI) (hp100, hpj2577, hpj2573, hpj2585, hp27248b); on ix86-at
   enabled by default */
#define CONFIG_HP100 1

/* option hp: Linux device driver for Ethernet controller HP PCLAN (27245 and
   other 27xxx series); on ix86-at enabled by default */
#define CONFIG_HPLAN 1

/* option hp-plus: Linux device driver for Ethernet controller HP PCLAN+
   (27247B and 27252A); on ix86-at enabled by default */
#define CONFIG_HPLAN_PLUS 1

/* option i82365: Linux device driver for Intel 82365 PC Card controller; on
   ix86-at enabled by default */
#define CONFIG_I82365 1

/* CONFIG_INET */
#define CONFIG_INET 1

/* option intel-gige: Linux device driver for Ethernet controller Intel PCI
   Gigabit Ethernet; on ix86-at enabled by default */
#define CONFIG_INTEL_GIGE 1

/* option pcmcia-isa: isa bus support in the pcmcia core; on ix86-at enabled
   by default */
#define CONFIG_ISA 1

/* option lance: Linux device driver for Ethernet controller AMD LANCE and
   PCnet (at1500, ne2100); on ix86-at enabled by default */
#define CONFIG_LANCE 1

/* 386 */
/* #undef CONFIG_M386 */

/* 486 */
/* #undef CONFIG_M486 */

/* 586 */
/* #undef CONFIG_M586 */

/* 686 */
#define CONFIG_M686 1

/* option myson803: Linux device driver for Ethernet controller Myson MTD803
   Ethernet adapter series; on ix86-at enabled by default */
#define CONFIG_MYSON803 1

/* option natsemi: Linux device driver for Ethernet controller National
   Semiconductor DP8381x series PCI Ethernet; on ix86-at enabled by default */
#define CONFIG_NATSEMI 1

/* option ne: Linux device driver for Ethernet controller NE2000/NE1000 ISA
   (ne, ne1000, ne2000); on ix86-at enabled by default and for qemu */
#define CONFIG_NE2000 1

/* option ne2k-pci: Linux device driver for Ethernet controller PCI NE2000; on
   ix86-at enabled by default */
#define CONFIG_NE2K_PCI 1

/* option ni52: Linux device driver for Ethernet controller NI5210; on ix86-at
   enabled by default */
#define CONFIG_NI52 1

/* option ni65: Linux device driver for Ethernet controller NI6510; on ix86-at
   enabled by default */
#define CONFIG_NI65 1

/* option nmclan_cs: Linux device driver for New Media Ethernet LAN PCMCIA
   cards; on ix86-at enabled by default */
#define CONFIG_NMCLAN_CS 1

/* option ns820: Linux device driver for Ethernet controller National
   Semiconductor DP8382x series PCI Ethernet; on ix86-at enabled by default */
#define CONFIG_NS820 1

/* option orinoco_cs: Linux device driver for Hermes or Prism 2 PCMCIA
   Wireless adapters (Orinoco); on ix86-at enabled by default */
#define CONFIG_ORINOCO_CS 1

/* CONFIG_PCMCIA */
#define CONFIG_PCMCIA 1

/* option pcnet32: Linux device driver for Ethernet controller AMD PCI PCnet32
   (PCI bus NE2100 cards); on ix86-at enabled by default */
#define CONFIG_PCNET32 1

/* option pcnet_cs: Linux device driver for NS8390-based PCMCIA cards; on
   ix86-at enabled by default */
#define CONFIG_PCNET_CS 1

/* option rtl8139: Linux device driver for Ethernet controller RealTek
   8129/8139 (rtl8129, rtl8139) (not 8019/8029!); on ix86-at enabled by
   default */
#define CONFIG_RTL8139 1

/* CONFIG_SCSI */
#define CONFIG_SCSI 1

/* option wd7000: Linux device driver for SCSI controller WD 7000 */
/* #undef CONFIG_SCSI_7000FASST */

/* option advansys: Linux device driver for SCSI controller AdvanSys; on
   ix86-at enabled by default */
#define CONFIG_SCSI_ADVANSYS 1

/* option aha152x: Linux device driver for SCSI controller Adaptec
   AHA-152x/2825 (aha152x, aha2825); on ix86-at enabled by default */
#define CONFIG_SCSI_AHA152X 1

/* option aha1542: Linux device driver for SCSI controller Adaptec AHA-1542 */
/* #undef CONFIG_SCSI_AHA1542 */

/* option aha1740: Linux device driver for SCSI controller Adaptec AHA-1740;
   on ix86-at enabled by default */
#define CONFIG_SCSI_AHA1740 1

/* option aic7xxx: Linux device driver for SCSI controller Adaptec AIC7xxx */
/* #undef CONFIG_SCSI_AIC7XXX */

/* option AM53C974: Linux device driver for SCSI controller AM53/79C974
   (am53c974, am79c974) */
/* #undef CONFIG_SCSI_AM53C974 */

/* option BusLogic: Linux device driver for SCSI controller BusLogic; on
   ix86-at enabled by default */
#define CONFIG_SCSI_BUSLOGIC 1

/* option tmscsim: Linux device driver for SCSI controller Tekram DC-390(T)
   (dc390, dc390t); on ix86-at enabled by default */
#define CONFIG_SCSI_DC390T 1

/* option dtc: Linux device driver for SCSI controller DTC3180/3280 (dtc3180,
   dtc3280); on ix86-at enabled by default */
#define CONFIG_SCSI_DTC3280 1

/* option eata: Linux device driver for SCSI controller EATA ISA/EISA/PCI (DPT
   and generic EATA/DMA-compliant boards) */
/* #undef CONFIG_SCSI_EATA */

/* option eata_dma: Linux device driver for SCSI controller EATA-DMA (DPT,
   NEC, AT&T, SNI, AST, Olivetti, Alphatronix) */
/* #undef CONFIG_SCSI_EATA_DMA */

/* option eata_pio: Linux device driver for SCSI controller EATA-PIO (old DPT
   PM2001, PM2012A) */
/* #undef CONFIG_SCSI_EATA_PIO */

/* option fdomain: Linux device driver for SCSI controller Future Domain 16xx;
   on ix86-at enabled by default */
#define CONFIG_SCSI_FUTURE_DOMAIN 1

/* option gdth: Linux device driver for GDT SCSI Disk Array Controller */
/* #undef CONFIG_SCSI_GDTH */

/* option g_NCR5380: Linux device driver for SCSI controller Generic
   NCR5380/53c400 (ncr5380, ncr53c400) */
/* #undef CONFIG_SCSI_GENERIC_NCR5380 */

/* option in2000: Linux device driver for SCSI controller Always IN 2000; on
   ix86-at enabled by default */
#define CONFIG_SCSI_IN2000 1

/* option NCR53c406a: Linux device driver for SCSI controller NCR53c406a chip
   */
/* #undef CONFIG_SCSI_NCR53C406A */

/* option 53c78xx: Linux device driver for SCSI controller NCR 53C7,8xx */
/* #undef CONFIG_SCSI_NCR53C7xx */

/* option ncr53c8xx: Linux device driver for SCSI controller NCR53C8XX
   (ncr53c8xx, dc390f, dc390u, dc390w); on ix86-at enabled by default */
#define CONFIG_SCSI_NCR53C8XX 1

/* scsi omit flashpoint */
/* #undef CONFIG_SCSI_OMIT_FLASHPOINT */

/* option pas16: Linux device driver for SCSI controller PAS16; on ix86-at
   enabled by default */
#define CONFIG_SCSI_PASS16 1

/* option ppa: Linux device driver for IOMEGA Parallel Port ZIP drive */
/* #undef CONFIG_SCSI_PPA */

/* option qlogicfas: Linux device driver for SCSI controller Qlogic FAS; on
   ix86-at enabled by default */
#define CONFIG_SCSI_QLOGIC_FAS 1

/* option qlogicisp: Linux device driver for SCSI controller Qlogic ISP; on
   ix86-at enabled by default */
#define CONFIG_SCSI_QLOGIC_ISP 1

/* option seagate: Linux device driver for SCSI controller Seagate ST02,
   Future Domain TMC-8xx; on ix86-at enabled by default */
#define CONFIG_SCSI_SEAGATE 1

/* option sym53c8xx: Linux device driver for SCSI controller Symbios 53C8XX;
   on ix86-at enabled by default */
#define CONFIG_SCSI_SYM53C8XX 1

/* option t128: Linux device driver for SCSI controller Trantor
   T128/T128F/T228 (t128, t128f, t228); on ix86-at enabled by default */
#define CONFIG_SCSI_T128 1

/* option u14-34f: Linux device driver for SCSI controller UltraStor 14F/34F;
   on ix86-at enabled by default */
#define CONFIG_SCSI_U14_34F 1

/* option ultrastor: Linux device driver for SCSI controller UltraStor; on
   ix86-at enabled by default */
#define CONFIG_SCSI_ULTRASTOR 1

/* option seeq8005: Linux device driver for Ethernet controller Seeq8005; on
   ix86-at enabled by default */
#define CONFIG_SEEQ8005 1

/* option sis900: Linux device driver for Ethernet controller SiS 900; on
   ix86-at enabled by default */
#define CONFIG_SIS900 1

/* option sk_g16: Linux device driver for Ethernet controller Schneider & Koch
   G16; on ix86-at enabled by default */
#define CONFIG_SK_G16 1

/* option smc91c92_cs: Linux device driver for SMC91c92-based PCMCIA cards; on
   ix86-at enabled by default */
#define CONFIG_SMC91C92_CS 1

/* option starfire: Linux device driver for Ethernet controller Adaptec
   Starfire network adapter; on ix86-at enabled by default */
#define CONFIG_STARFIRE 1

/* option sundance: Linux device driver for Ethernet controller Sundance ST201
   "Alta" PCI Ethernet; on ix86-at enabled by default */
#define CONFIG_SUNDANCE 1

/* option tlan: Linux device driver for Ethernet controller TI ThunderLAN; on
   ix86-at enabled by default */
#define CONFIG_TLAN 1

/* option smc-ultra: Linux device driver for Ethernet controller SMC Ultra; on
   ix86-at enabled by default */
#define CONFIG_ULTRA 1

/* option smc-ultra32: Linux device driver for Ethernet controller SMC
   Ultra32; on ix86-at enabled by default */
#define CONFIG_ULTRA32 1

/* option via-rhine: Linux device driver for Ethernet controller VIA Rhine; on
   ix86-at enabled by default */
#define CONFIG_VIA_RHINE 1

/* option 3c59x: Linux device driver for Ethernet controller 3Com 59x/90x
   (3c59x, 3c590, 3c592, 3c595, 3c597, 3c90x, 3c900, 3c905)
   "Vortex/Boomerang"; on ix86-at enabled by default */
#define CONFIG_VORTEX 1

/* option wavelan: Linux device driver for Ethernet controller AT&T WaveLAN &
   DEC RoamAbout DS */
/* #undef CONFIG_WAVELAN */

/* option wd: Linux device driver for Ethernet controller WD80x3; on ix86-at
   enabled by default */
#define CONFIG_WD80x3 1

/* option winbond-840: Linux device driver for Ethernet controller Winbond
   W89c840 PCI Ethernet; on ix86-at enabled by default */
#define CONFIG_WINBOND840 1

/* CONFIG_WIRELESS */
#define CONFIG_WIRELESS 1

/* option xirc2ps_cs: Linux device driver for Xircom CreditCard and Realport
   PCMCIA ethernet; on ix86-at enabled by default */
#define CONFIG_XIRC2PS_CS 1

/* option yellowfin: Linux device driver for Ethernet controller Packet
   Engines Yellowfin Gigabit-NIC; on ix86-at enabled by default */
#define CONFIG_YELLOWFIN 1

/* option znet: Linux device driver for Ethernet controller Zenith Z-Note
   (znet, znote); on ix86-at enabled by default */
#define CONFIG_ZNET 1

/* CPU */
#define CPU 686

/* CPU_L1_SHIFT */
#define CPU_L1_SHIFT 6

/* ENABLE_IMMEDIATE_CONSOLE */
#define ENABLE_IMMEDIATE_CONSOLE 0

/* FAST_TAS */
#define FAST_TAS 0

/* HW_FOOTPRINT */
#define HW_FOOTPRINT 0

/* KERNEL */
#define KERNEL 1

/* Linux device drivers. */
#define LINUX_DEV 1

/* MACH */
#define MACH 1

/* MACH_COUNTERS */
#define MACH_COUNTERS 0

/* MACH_DEBUG */
#define MACH_DEBUG 1

/* MACH_FIXPRI */
#define MACH_FIXPRI 1

/* MACH_HOST */
#define MACH_HOST 0

/* be a hypervisor guest */
/* #undef MACH_HYP */

/* MACH_IPC_DEBUG */
#define MACH_IPC_DEBUG 1

/* MACH_IPC_TEST */
#define MACH_IPC_TEST 0

/* Use the in-kernel debugger? */
#define MACH_KDB 0

/* Standalone MACH kernel */
#define MACH_KERNEL 1

/* MACH_KERNSAMPLE */
#define MACH_KERNSAMPLE 0

/* enable use of kmsg device */
#define MACH_KMSG /**/

/* MACH_LDEBUG */
#define MACH_LDEBUG 0

/* MACH_LOCK_MON */
#define MACH_LOCK_MON 0

/* lpr device */
#define MACH_LPR /**/

/* MACH_MACHINE_ROUTINES */
#define MACH_MACHINE_ROUTINES 1

/* MACH_MP_DEBUG */
#define MACH_MP_DEBUG 0

/* MACH_PAGEMAP */
#define MACH_PAGEMAP 1

/* MACH_PCSAMPLE */
#define MACH_PCSAMPLE 1

/* Enable pseudo physical memory support */
/* #undef MACH_PSEUDO_PHYS */

/* Enable paravirtualized segment descriptors support */
/* #undef MACH_PV_DESCRIPTORS */

/* Enable paravirtualized page tables support */
/* #undef MACH_PV_PAGETABLES */

/* Enable ring1 kernel support */
/* #undef MACH_RING1 */

/* MACH_TTD */
#define MACH_TTD 0

/* MACH_VM_DEBUG */
#define MACH_VM_DEBUG 1

/* build a MachXen kernel */
/* #undef MACH_XEN */

/* set things up for a multiprocessor */
/* #undef MULTIPROCESSOR */

/* NCOM */
#define NCOM 2

/* number of CPUs */
#define NCPUS 1

/* Define to 1 if assertions should be disabled. */
/* #undef NDEBUG */

/* NLPR */
#define NLPR 1

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "bug-hurd@gnu.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "GNU Mach"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "GNU Mach 1.8"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "gnumach"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.8"

/* PAE support */
/* #undef PAE */

/* POWER_SAVE */
#define POWER_SAVE 1

/* SLAB_USE_CPU_POOLS */
#define SLAB_USE_CPU_POOLS 0

/* SLAB_VERIFY */
#define SLAB_VERIFY 0

/* STAT_TIME */
#define STAT_TIME 1

/* Define to enable tickless timer operation */
/* #undef TICKLESS_TIMER */

/* enable 32-bit user on 64-bit kernel */
/* #undef USER32 */

/* XPR_DEBUG */
#define XPR_DEBUG 0

/* __ELF__ */
#define __ELF__ 1

/* __KERNEL__ */
#define __KERNEL__ 1

/* __SMP__ */
/* #undef __SMP__ */
