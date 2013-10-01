#!/bin/sh

ORIG=ms_gswap.3
LIST="ms_gswap2.3 ms_gswap3.3 ms_gswap4.3 ms_gswap8.3 ms_gswap2a.3 ms_gswap4a.3 ms_gswap8a.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_doy2md.3
LIST="ms_md2doy.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_genfactmult.3
LIST="ms_ratapprox.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_lookup.3
LIST="ms_errorstr.3 ms_samplesize.3 ms_encodingstr.3 ms_blktdesc.3 ms_blktlen.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_log.3
LIST="ms_log_l.3 ms_loginit.3 ms_loginit_l.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_readmsr.3
LIST="ms_readmsr_r.3 ms_readtraces.3 ms_readtraces_timewin.3 ms_readtraces_selection.3 ms_readtracelist.3 ms_readtracelist_timewin.3 ms_readtracelist_selection.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_srcname.3
LIST="ms_recsrcname.3 msr_srcname.3 mst_srcname.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_strncpclean.3
LIST="ms_strncpopen.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_time.3
LIST="ms_btime3hptime.3 ms_btime2isotimestr.3 ms_btime2mdtimestr.3 ms_btime2seedtimestr.3 ms_hptime2btime.3 ms_hptime2isotimestr.3 ms_hptime2mdtimestr.3 ms_hptime2seedtimestr.3 ms_time2hptime.3 ms_seedtimestr2hptime.3 ms_timestr2hptime.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=msr_init.3
LIST="msr_free.3 msr_free_blktchain.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=msr_pack.3
LIST="msr_pack_header.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=msr_samprate.3
LIST="msr_nomsamprate.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=msr_starttime.3
LIST="msr_starttime_uc.3 msr_endtime.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=mst_addmsr.3
LIST="mst_addspan.3 mst_addmsrtogroup.3 mst_addtracetogroup.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=mst_findmatch.3
LIST="mst_findadjacent.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=mst_groupsort.3
LIST="mst_groupheal.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=mst_init.3
LIST="mst_free.3 mst_initgroup.3 mst_freegroup.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=mst_pack.3
LIST="mst_packgroup.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=mst_printtracelist.3
LIST="mst_printsynclist.3 mst_printgaplist.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=mst_convertsamples.3
LIST="mstl_convertsamples.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=mstl_init.3
LIST="mstl_free.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=mstl_printtracelist.3
LIST="mstl_printsynclist.3 mstl_printgaplist.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_selection.3
LIST="ms_freeselections.3 ms_addselect.3 ms_addselect_comp.3 ms_matchselect.3 ms_readselectionsfile.3 ms_printselections.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=msr_parse.3
LIST="msr_parse_selection.3 ms_detect.3"
for link in $LIST ; do
    ln -s $ORIG $link
done

ORIG=ms_writemseed.3
LIST="msr_writemseed.3 mst_writemseed.3 mst_writemseedgroup.3"
for link in $LIST ; do
    ln -s $ORIG $link
done
