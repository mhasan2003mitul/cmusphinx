#!/bin/bash -x

# While "loopUntilSuccess.sh" is not part of the regression tests, use
# the local copy. That's why we need the ${HOME}/script here
export PATH=/usr/local/bin:${PATH}:${HOME}/script

# Check that we have all executables
if ! WGET=`command -v wget 2>&1`; then exit 1; fi
if ! SVN=`command -v svn 2>&1`; then exit 1; fi
if ! PERL=`command -v perl 2>&1`; then exit 1; fi
if ! MAKE=`command -v gmake 2>&1`; then 
  if ! MAKE=`command -v make 2>&1`; then exit 1; fi
fi
if ! TAR=`command -v gtar 2>&1`; then
  if ! TAR=`command -v tar 2>&1`; then exit 1; fi
fi
if ! MAIL=`command -v mhmail 2>&1`; then
  if ! MAIL=`command -v mailx 2>&1`; then
    if ! MAIL=`command -v sendmail 2>&1`; then
      if ! MAIL=`command -v mutt 2>&1`; then exit 1; fi
    fi
  fi
fi

# Create temp directory
temp_dir=/tmp/temp$$
mkdir $temp_dir
pushd $temp_dir > /dev/null

LOG=$temp_dir/log.txt
echo > $LOG
MAILLIST=cmusphinx-results@lists.sourceforge.net

# Get the data
${WGET} -q http://www.speech.cs.cmu.edu/databases/an4/an4_sphere.tar.gz
${TAR} -xzf an4_sphere.tar.gz
/bin/rm an4_sphere.tar.gz

# Get sphinxbase
if (loopUntilSuccess.sh ${SVN} co https://svn.sourceforge.net/svnroot/cmusphinx/trunk/sphinxbase > /dev/null &&
cd sphinxbase &&
./autogen.sh &&
./autogen.sh &&
make ; then

# Get the trainer
if (loopUntilSuccess.sh ${SVN} co https://svn.sourceforge.net/svnroot/cmusphinx/trunk/SphinxTrain > /dev/null &&
cd SphinxTrain &&
./configure DISTCHECK_CONFIGURE_FLAGS=--with-sphinxbase=$temp_dir/sphinxbase && 
${MAKE} && 
${PERL} scripts_pl/setup_tutorial.pl an4) >> $LOG 2>&1 ; then

# Get the decoder
if (loopUntilSuccess.sh ${SVN} co https://svn.sourceforge.net/svnroot/cmusphinx/trunk/sphinx3 > /dev/null &&
cd sphinx3 &&
./autogen.sh &&
./autogen.sh --prefix=`pwd`/build DISTCHECK_CONFIGURE_FLAGS=--with-sphinxbase=$temp_dir/sphinxbase && 
${MAKE} all install && 
${PERL} scripts/setup_tutorial.pl an4) >> $LOG 2>&1 ; then

# Run it
cd an4 &&
perl scripts_pl/make_feats.pl -ctl etc/an4_train.fileids >> $LOG 2>&1 &&
perl scripts_pl/make_feats.pl -ctl etc/an4_test.fileids >> $LOG 2>&1 &&
perl scripts_pl/RunAll.pl >> $LOG 2>&1 &&
perl scripts_pl/decode/slave.pl >> $LOG 2>&1

# end of if (sphinx3)
fi
# end of if (SphinxTrain)
fi
# end of sphinxbase
fi

# Check whether the Word Error Rate is reasonable, hardwired for now
if SER=`grep 'SENTENCE ERROR' $LOG 2>&1`; then
FAIL=`echo $SER | awk '{if ($(NF-1) - 55 > 2) print "1"}'`;
else
FAIL=1
fi;

# Send mail if we failed
if test x${FAIL} != x ; then
$MAIL -s "Tutorial failed" ${MAILLIST} < ${LOG}
else
echo $SER | $MAIL -s "Tutorial succeded" ${MAILLIST}
fi

# Remove what we created
popd > /dev/null
/bin/rm -rf $temp_dir