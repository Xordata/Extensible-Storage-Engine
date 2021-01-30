// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "std.hxx"


#ifdef PERFMON_SUPPORT

PERFInstanceDelayedTotal<> cPIBInUse;
LONG LPIBInUseCEFLPv( LONG iInstance, void* pvBuf )
{
    cPIBInUse.PassTo( iInstance, pvBuf );
    return 0;
}

PERFInstanceDelayedTotal<LONG, INST, fFalse> cPIBTotal;
LONG LPIBTotalCEFLPv( LONG iInstance, void* pvBuf )
{
    cPIBTotal.PassTo( iInstance, pvBuf );
    return 0;
}

#endif

TrxidStack::TrxidStack() : m_ctrxCurr( 0 )
{
}

TrxidStack::~TrxidStack()
{
}

void TrxidStack::Push( const TRXID trxid )
{
    AssertPREFIX( m_ctrxCurr < _countof( m_rgtrxid ) );
    AssertPREFIX( m_ctrxCurr < _countof( m_rgtrxtime ) );
    EnforceSz( m_ctrxCurr < m_ctrxMax, "TooManyTransactionLevels" );
    m_rgtrxid[m_ctrxCurr] = trxid;
    m_rgtrxtime[m_ctrxCurr] = UtilGetCurrentFileTime();
    ++m_ctrxCurr;
}

void TrxidStack::Pop()
{
    Assert( m_ctrxCurr > 0 );
    Enforce( m_ctrxCurr > 0 );
    --m_ctrxCurr;
}

void TrxidStack::Clear()
{
    m_ctrxCurr = 0;
}

ERR TrxidStack::ErrDump( __out_ecount_z( cchBuf ) WCHAR * const wszBuf, const size_t cchBuf, const WCHAR * const cwszLineBreak ) const
{
    Assert( wszBuf );
    Assert( cchBuf > 0 );

    ERR err;
    
    size_t cchRemaining = cchBuf;
    WCHAR * wsz = wszBuf;

    wszBuf[0] = 0;
    for(INT itrx = m_ctrxCurr-1; itrx >= 0; --itrx)
    {
        WCHAR wszId[64] = {0};
        WCHAR wszTimeOut[64] = {0};
        
        size_t cchTimeOut;
        Call( ErrOSStrCbFormatW( wszId, sizeof( wszId ), L"%I64d@", m_rgtrxid[itrx] ) );
        Call( ErrUtilFormatFileTimeAsTimeWithSeconds( m_rgtrxtime[itrx], wszTimeOut, _countof( wszTimeOut ), &cchTimeOut ) );

        AssertPREFIX( cchRemaining <= cchBuf );
        Call( ErrOSStrCbAppendW( wsz, cchRemaining*sizeof(WCHAR), wszId ) );
        const LONG cchId = LOSStrLengthW( wszId );
        wsz += cchId;
        cchRemaining -= cchId;
        if ( cchRemaining <= 0 )
        {
            Error( ErrERRCheck( JET_errBufferTooSmall ) );
        }
        
        Call( ErrOSStrCbAppendW( wsz, cchRemaining*sizeof(WCHAR), wszTimeOut ) );
        const LONG cchTime = LOSStrLengthW( wszTimeOut );
        wsz += cchTime;
        cchRemaining -= cchTime;
        if ( cchRemaining <= 0 )
        {
            Error( ErrERRCheck( JET_errBufferTooSmall ) );
        }

        Call( ErrOSStrCbAppendW( wsz, cchRemaining*sizeof(WCHAR), cwszLineBreak ) );
        const LONG cchNewLine = LOSStrLengthW( cwszLineBreak );
        wsz += cchNewLine;
        cchRemaining -= cchNewLine;
        if ( cchRemaining <= 0 )
        {
            Error( ErrERRCheck( JET_errBufferTooSmall ) );
        }
    }

    return JET_errSuccess;

HandleError:
    wszBuf[0] = 0;
    return err;
}

#ifdef ENABLE_JET_UNIT_TEST

JETUNITTEST( TrxidStack, PushAndPop )
{
    TrxidStack stack;
    stack.Push( 57061) ;
    stack.Pop();
}

JETUNITTEST( TrxidStack, PushAndClear )
{
    TrxidStack stack;
    stack.Push( 57061) ;
    stack.Clear();
}

JETUNITTEST( TrxidStack, Dump )
{
    TrxidStack stack;
    stack.Push( 999 );

    WCHAR wsz[32];
    CHECK( JET_errSuccess == stack.ErrDump( wsz, _countof( wsz ) ) );
    CHECK( 0 == wcsncmp(wsz, L"999@", 4 ) );
}

JETUNITTEST( TrxidStack, BufferTooSmallForTrxid )
{
    TrxidStack stack;
    stack.Push( 999 );

    WCHAR wsz[3];
    CHECK( JET_errBufferTooSmall == stack.ErrDump( wsz, _countof( wsz ) ) );
    CHECK( 0 == wsz[0] );
}

JETUNITTEST( TrxidStack, BufferTooSmallForTime )
{
    TrxidStack stack;
    stack.Push( 999 );

    WCHAR wsz[5];
    CHECK( JET_errBufferTooSmall == stack.ErrDump( wsz, _countof( wsz ) ) );
    CHECK( 0 == wsz[0] );
}

#endif

template<class KEY, class DATA>
JET_ERR ErrFromRedBlackTreeErr(typename CRedBlackTree<KEY,DATA>::ERR err)
{
    JET_ERR jeterr;
    
    typedef CRedBlackTree<KEY,DATA> Tree;
    switch(err)
    {
        case Tree::ERR::errSuccess:
            jeterr = JET_errSuccess;
            break;
        case Tree::ERR::errOutOfMemory:
            jeterr = ErrERRCheck( JET_errOutOfMemory );
            break;
        case Tree::ERR::errDuplicateEntry:
            jeterr = ErrERRCheck( JET_errKeyDuplicate );
            break;
        case Tree::ERR::errEntryNotFound:
            jeterr = ErrERRCheck( JET_errRecordNotFound );
            break;
        default:
            AssertSz(fFalse, "Unknown RedBlackTree error");
            jeterr = ErrERRCheck( JET_errInternalError );
            break;
    }
    return jeterr;
}

ERR PIB::ErrRegisterDeferredRceid( const RCEID& rceid, PGNO pgno )
{
    Assert( rceidNull != rceid );
    return ErrFromRedBlackTreeErr<RCEID,PGNO>( m_redblacktreeRceidDeferred.ErrInsert( rceid, pgno ) );
}


ERR PIB::ErrDeregisterDeferredRceid( const RCEID& rceid )
{
    Assert( rceidNull != rceid );
    (void)m_redblacktreeRceidDeferred.ErrDelete( rceid );
    return JET_errSuccess;
}


VOID PIB::RemoveAllDeferredRceid()
{
    m_redblacktreeRceidDeferred.MakeEmpty();
}


VOID PIB::AssertNoDeferredRceid() const
{
    AssertRTL( m_redblacktreeRceidDeferred.FEmpty() );
}

ERR PIB::ErrRegisterRceid( const RCEID rceid, RCE * const prce)
{
    return ErrFromRedBlackTreeErr<RCEID,RCE*>( m_redblacktreePrceOfSession.ErrInsert( rceid, prce ) );
}

ERR PIB::ErrDeregisterRceid( const RCEID rceid )
{
    (void)m_redblacktreePrceOfSession.ErrDelete( rceid );
    return JET_errSuccess;
}

VOID PIB::RemoveAllRceid()
{
    m_redblacktreePrceOfSession.MakeEmpty();
}

RCE * PIB::PrceNearestRegistered( const RCEID rceid ) const
{
    RCEID rceidT;
    RCE* prceT;
    if(CRedBlackTree<RCEID,RCE*>::ERR::errSuccess == m_redblacktreePrceOfSession.ErrFindNearest(rceid, &rceidT, &prceT))
    {
        return prceT;
    }
    else
    {
        return NULL;
    }
}

void PIB::SetLevel( const LEVEL level )
{
    while( m_level > level)
    {
        m_trxidstack.Pop();
        m_level--;
    }
    while( m_level < level)
    {
        m_trxidstack.Push( 53285 );
        m_level++;
    }
    Assert( level == m_level );
}


void PIB::IncrementLevel( const TRXID trxid )
{
    m_trxidstack.Push( trxid );
    ++m_level;
    Assert(m_level <= levelMax);
}

void PIB::DecrementLevel()
{
    m_trxidstack.Pop();
    --m_level;
    Assert(m_level >= 0);
}
    
TRX TrxOldestCached( const INST * const pinst )
{
    return pinst->TrxOldestCached();
}

#ifdef PERFMON_SUPPORT
PERFInstanceDelayedTotal<QWORD, INST, fFalse, fFalse> lOldestTransaction;

LONG LOldestTransactionCEFLPv( LONG iInstance, VOID *pvBuf )
{
    lOldestTransaction.PassTo( iInstance, pvBuf );
    return 0;
}
#endif

void UpdateCachedTrxOldest( INST * const pinst )
{
    TRX             trxOldest   = trxMax;
    TICK            tickOldestTransaction = TickOSTimeCurrent();
    size_t          iProc;
    const size_t    cProcs      = (size_t)OSSyncGetProcessorCountMax();

    for ( iProc = 0; iProc < cProcs; iProc++ )
    {
        INST::PLS* const ppls = pinst->Ppls( iProc );
        ppls->m_rwlPIBTrxOldest.EnterAsReader();
    }
    for ( iProc = 0; iProc < cProcs; iProc++ )
    {
        INST::PLS* const ppls = pinst->Ppls( iProc );
        PIB* const ppibTrxOldest = ppls->m_ilTrxOldest.PrevMost();
        if ( ppibTrxOldest && TrxCmp( ppibTrxOldest->trxBegin0, trxOldest ) < 0 )
        {
            trxOldest = ppibTrxOldest->trxBegin0;
            tickOldestTransaction = ppibTrxOldest->TickLevel0Begin();
        }
    }
    Enforce( trxOldest == trxMax || TrxCmp( trxOldest, pinst->m_trxNewest + TRXID_INCR/2 ) <= 0 );
    PERFOpt( lOldestTransaction.Set( pinst, DtickDelta( tickOldestTransaction, TickOSTimeCurrent() ) ) );
    pinst->SetTrxOldestCached(trxOldest);
    for ( iProc = 0; iProc < cProcs; iProc++ )
    {
        INST::PLS* const ppls = pinst->Ppls( iProc );
        ppls->m_rwlPIBTrxOldest.LeaveAsReader();
    }
}

TRX TrxOldest( INST *pinst )
{
    UpdateCachedTrxOldest( pinst );
    return TrxOldestCached( pinst );
}


ERR PIB::ErrAbortAllMacros( BOOL fLogEndMacro )
{
    ASSERT_VALID( this );

    ERR     err;
    MACRO   *pMacro, *pMacroNext;
    for ( pMacro = m_pMacroNext; pMacro != NULL; pMacro = pMacroNext )
    {
        pMacroNext = pMacro->PMacroNext();
        if ( pMacro->Dbtime() != dbtimeNil )
        {
            const DBTIME    dbtime  = pMacro->Dbtime();

            if ( fLogEndMacro )
            {
                LGPOS       lgpos;
                CallR( ErrLGMacroAbort( this, dbtime, &lgpos ) );
            }

            ResetMacroGoing( dbtime );
        }
        else
        {
            Assert( pMacro == m_pMacroNext );
        }
    }

    if ( NULL != m_pMacroNext )
    {
        Assert( m_pMacroNext->PMacroNext() == NULL );
        Assert( dbtimeNil == m_pMacroNext->Dbtime() );
        m_pMacroNext->ReleaseBuffer();
        OSMemoryHeapFree( m_pMacroNext );
        m_pMacroNext = NULL;
    }

    return JET_errSuccess;
}

VOID PIB::ResolveCachePriorityForDb_( const DBID dbid )
{
    Assert( m_critCachePriority.FOwner() );
    Assert( dbid < _countof( m_rgpctCachePriority ) );


    const WORD pctPibCachePriority = m_pctCachePriority;
    const BOOL fIsPibCachePriorityAssigned = FIsCachePriorityAssigned( pctPibCachePriority );

    WORD pctFmpCachePriority = g_pctCachePriorityUnassigned;
    const IFMP ifmp = m_pinst->m_mpdbidifmp[ dbid ];
    if ( FMP::FAllocatedFmp( ifmp ) )
    {
        const FMP* const pfmp = &g_rgfmp[ ifmp ];
        if ( pfmp->FInUse() )
        {
            pctFmpCachePriority = pfmp->PctCachePriorityFmp();
        }
    }
    const BOOL fIsFmpCachePriorityAssigned = FIsCachePriorityAssigned( pctFmpCachePriority );

    WORD pctCachePriority = g_pctCachePriorityUnassigned;

    if ( !fIsFmpCachePriorityAssigned && !fIsPibCachePriorityAssigned )
    {
        pctCachePriority = (WORD)PctINSTCachePriority( m_pinst );
        goto Return;
    }

    if ( !fIsFmpCachePriorityAssigned && fIsPibCachePriorityAssigned )
    {
        pctCachePriority = pctPibCachePriority;
        goto Return;
    }

    if ( fIsFmpCachePriorityAssigned && !fIsPibCachePriorityAssigned )
    {
        pctCachePriority = pctFmpCachePriority;
        goto Return;
    }

    Assert( fIsFmpCachePriorityAssigned && fIsPibCachePriorityAssigned );
    pctCachePriority = min( pctPibCachePriority, pctFmpCachePriority );

Return:
    Assert( FIsCachePriorityValid( pctCachePriority ) );

    m_rgpctCachePriority[ dbid ] = pctCachePriority;
}

ERR PIB::ErrSetCachePriority( const void* const pvCachePriority, const INT cbCachePriority )
{
    if ( cbCachePriority != sizeof( DWORD ) )
    {
        return ErrERRCheck( JET_errInvalidBufferSize );
    }

    static_assert( sizeof( m_pctCachePriority ) <= sizeof( DWORD ), "Cache priority should be a DWORD or smaller" );

    const DWORD pctCachePriority = *( (DWORD *) pvCachePriority );
    if ( !FIsCachePriorityValid( pctCachePriority ) )
    {
        return ErrERRCheck( JET_errInvalidParameter );
    }

    m_critCachePriority.Enter();

    m_pctCachePriority = (WORD)pctCachePriority;
    for ( DBID dbid = 0; dbid < _countof( m_rgpctCachePriority ); dbid++ )
    {
        ResolveCachePriorityForDb_( dbid );
    }
    
#ifdef DEBUG
    Assert( FIsCachePriorityValid( m_pctCachePriority ) );
    for ( DBID dbid = 0; dbid < _countof( m_rgpctCachePriority ); dbid++ )
    {
        Assert( FIsCachePriorityValid( m_rgpctCachePriority[ dbid ] ) );
    }
#endif

    m_critCachePriority.Leave();

    return JET_errSuccess;
}

VOID PIB::ResolveCachePriorityForDb( const DBID dbid )
{
    m_critCachePriority.Enter();
    ResolveCachePriorityForDb_( dbid );
    m_critCachePriority.Leave();
}

WORD PIB::PctCachePriorityPib() const
{
    WORD pctCachePriority = m_pctCachePriority;

    if ( FIsCachePriorityAssigned( pctCachePriority ) )
    {
        Assert( FIsCachePriorityValid( pctCachePriority ) );
        return pctCachePriority;
    }

    pctCachePriority = (WORD)PctINSTCachePriority( m_pinst );
    Assert( FIsCachePriorityValid( pctCachePriority ) );

    return pctCachePriority;
}

ERR ErrPIBInit( INST *pinst )
{
    ERR err = JET_errSuccess;

    pinst->m_ppibGlobal = ppibNil;

    if ( pinst->FRecovering() )
    {
        CallS( ErrRESSetResourceParam( pinst, JET_residPIB, JET_resoperEnableMaxUse, fFalse ) );
    }
    Call( pinst->m_cresPIB.ErrInit( JET_residPIB ) );


    PERFOpt( cPIBInUse.Clear( pinst ) );
    DWORD_PTR cPIBQuota;
    CallS( ErrRESGetResourceParam( pinst, JET_residPIB, JET_resoperMaxUse, &cPIBQuota ) );
    PERFOpt( cPIBTotal.Set( pinst, (LONG)cPIBQuota ) );

HandleError:
    if ( err < JET_errSuccess )
    {
        PIBTerm( pinst );
    }
    return err;
}

VOID PIBTerm( INST *pinst )
{
    PIB     *ppib;

    PERFOpt( cPIBTotal.Clear( pinst ) );
    PERFOpt( cPIBInUse.Clear( pinst ) );

    pinst->m_critPIB.Enter();
    while ( ppib = pinst->m_ppibGlobal )
    {
        Assert( !ppib->FLGWaiting() );

        if( FPIBUserOpenedDatabase( ppib, dbidTemp ) )
        {
            DBResetOpenDatabaseFlag( ppib, pinst->m_mpdbidifmp[ dbidTemp ] );
        }

        while( pfucbNil != ppib->pfucbOfSession )
        {
            FUCB    *pfucb  = ppib->pfucbOfSession;

            if ( FFUCBSort( pfucb ) )
            {
                Assert( !( FFUCBIndex( pfucb ) ) );
                CallS( ErrIsamSortClose( ppib, pfucb ) );
            }
            else
            {
                AssertSz( fFalse, "PIBTerm: At least one non sort fucb is left open." );
                break;
            }
        }

        pinst->m_ppibGlobal = ppib->ppibNext;
        delete ppib;
    }
    pinst->m_critPIB.Leave();

    pinst->m_cresPIB.Term();
    if ( pinst->FRecovering() )
    {
        CallS( ErrRESSetResourceParam( pinst, JET_residPIB, JET_resoperEnableMaxUse, fTrue ) );
    }
}


ERR ErrPIBBeginSession( INST *pinst, _Outptr_ PIB ** pppib, PROCID procidTarget, BOOL fForCreateTempDatabase )
{
    ERR     err     = JET_errSuccess;
    PIB*    ppib    = NULL;

    Assert( pinst->FRecovering() || procidTarget == procidNil );

    ppib = new( pinst ) PIB;
    if ( ppib == NULL )
    {
        err = ErrERRCheck( JET_errOutOfSessions );
        goto HandleError;
    }

    if ( procidNil != procidTarget )
    {
        Assert( pinst->FRecovering() );
        Assert( !FPIBUserOpenedDatabase( ppib, dbidTemp ) );
    }
    Assert( !FSomeDatabaseOpen( ppib ) );
    Assert( ppib->pfucbOfSession == pfucbNil );
    Assert( 0 == ppib->cCursors );

    ppib->m_pinst = pinst;
    Assert( 0 == ppib->Level() );

    Assert( ppib->grbitCommitDefault == NO_GRBIT ); 

    
    const JET_GRBIT grbitNonSysFlags = UlParam( pinst, JET_paramIOPriority ) & ~( JET_IOPriorityLowForCheckpoint | JET_IOPriorityLowForScavenge );
    CallS( ppib->ErrSetUserIoPriority( &grbitNonSysFlags, sizeof( grbitNonSysFlags ) ) );

    if ( !fForCreateTempDatabase
        && UlParam( pinst, JET_paramMaxTemporaryTables ) > 0
        && !pinst->FRecovering()
        && pinst->m_fTempDBCreated )
    {
        
        DBSetOpenDatabaseFlag( ppib, pinst->m_mpdbidifmp[ dbidTemp ] );
    }

    
    ppib->lgposStart = lgposMax;
    ppib->trxBegin0 = trxMax;

    ppib->lgposCommit0 = lgposMax;
    ppib->lgposWaitLogWrite = lgposMax;

    Assert( !ppib->FLoggedCheckpointGettingDeep() );
    ppib->ResetLoggedCheckpointGettingDeep();

    
    Assert( !ppib->FUserSession() );
    Assert( !ppib->FAfterFirstBT() );
    Assert( !ppib->FRecoveringEndAllSessions() );
    Assert( !ppib->FLGWaiting() );
    Assert( !ppib->FBegin0Logged() );
    Assert( !ppib->FSetAttachDB() );
    Assert( !ppib->FSessionOLD() );
    Assert( !ppib->FSessionOLD2() );
    Assert( !ppib->FSessionDBScan() );
    Assert( ppib->levelBegin == 0 );
    Assert( ppib->clevelsDeferBegin == 0 );
    Assert( ppib->levelRollback == 0 );
    Assert( ppib->updateid == updateidNil );

    Assert( dwPIBSessionContextNull == ppib->dwSessionContext
        || dwPIBSessionContextUnusable == ppib->dwSessionContext );
    Assert( ppib->dwSessionContext == dwPIBSessionContextNull );
    Assert( ppib->dwSessionContextThreadId == 0 );
    Assert( ppib->dwTrxContext == 0 );
    Assert( ppib->m_cInJetAPI == 0 );

    Assert( ppib->CbClientCommitContextGeneric() == 0 );

    if ( pinst->FRecovering() && fRecoveringUndo != pinst->m_plog->FRecoveringMode() &&
         procidTarget == procidNil )
    {
        procidTarget = procidReadDuringRecovery;
    }

    
    PIB** pppibNext;
    pinst->m_critPIB.Enter();
    if ( procidTarget == procidNil )
    {
        PROCID procidNext;
        for (   pppibNext = (PIB**)&pinst->m_ppibGlobal, procidNext = 1;
                *pppibNext && (*pppibNext)->procid == procidNext;
                pppibNext = &( (*pppibNext)->ppibNext ), procidNext++ );
#ifdef DEBUG
        DWORD_PTR cPIBQuota;
        CallS( ErrRESGetResourceParam( pinst, JET_residPIB, JET_resoperMaxUse, &cPIBQuota ) );
        Assert( procidNext > 0 && procidNext <= cPIBQuota );
#endif
        ppib->procid = procidNext;
    }
    else
    {
        for (   pppibNext = (PIB**)&pinst->m_ppibGlobal;
                *pppibNext && (*pppibNext)->procid < procidTarget;
                pppibNext = &( (*pppibNext)->ppibNext ) );
        ppib->procid = procidTarget;
    }

    
    ppib->ppibNext = *pppibNext;
    *pppibNext = ppib;
#ifdef DEBUG
    if ( !pinst->FRecovering() )
    {
        PIB* ppibT;
        for (   ppibT = pinst->m_ppibGlobal;
                ppibT && ppibT->ppibNext && ppibT->procid < ppibT->ppibNext->procid;
                ppibT = ppibT->ppibNext );
        Assert( !ppibT || !ppibT->ppibNext );
    }
#endif

    ppib->m_pctCachePriority = (WORD)g_pctCachePriorityUnassigned;
    for ( DBID dbid = 0; dbid < _countof( pinst->m_mpdbidifmp ); dbid++ )
    {
        ppib->ResolveCachePriorityForDb( dbid );
    }

    pinst->m_critPIB.Leave();

    PERFOpt( cPIBInUse.Inc( pinst ) );

    *pppib = ppib;

HandleError:
    return err;
}


VOID PIBEndSession( PIB *ppib )
{
    INST*   pinst = PinstFromPpib( ppib );
    PIB**   pppib = NULL;
    BOOL    fLeakPIB = fFalse;

    Assert( dwPIBSessionContextNull == ppib->dwSessionContext
        || dwPIBSessionContextUnusable == ppib->dwSessionContext );

    
    for ( FUCB* pfucb = ppib->pfucbOfSession; pfucb != pfucbNil; pfucb = pfucb->pfucbNextOfSession )
    {
        AssertSz( FFUCBDeferClosed( pfucb ) &&
            ( ppib->ErrRollbackFailure() < JET_errSuccess ), "Unexpected open cursor." );
        RFSSetKnownResourceLeak();
        fLeakPIB = fTrue;
    }

    if ( fLeakPIB )
    {
        return;
    }

    
    pinst->m_critPIB.Enter();
    for ( pppib = (PIB**) &pinst->m_ppibGlobal; *pppib != ppib && *pppib != ppibNil; pppib = &( ( *pppib )->ppibNext ) );
    Assert( *pppib != ppibNil );
    *pppib = ( *pppib )->ppibNext;
    pinst->m_critPIB.Leave();

    for ( DBID dbid = dbidUserLeast; dbid < dbidMax; dbid++ )
    {
        AssertSzRTL( ( ( ppib )->rgcdbOpen[ dbid ] ) == 0, "This means we're closing an internal PIB without closing all databases off it.  This DB will be impossible to detach/term later, that's bad." );
    }

    if ( ppib->m_cInJetAPI )
    {
        AssertRTL( ppib->Putc() == PutcTLSGetUserContext() );
        TLSSetUserTraceContext( NULL );
    }

    delete ppib;

    PERFOpt( cPIBInUse.Dec( pinst ) );
}

ERR ErrPIBOpenTempDatabase( PIB *ppib )
{
    ERR err = JET_errSuccess;
    Assert( ppib != NULL );

    if ( !FPIBUserOpenedDatabase( ppib, dbidTemp ) )
    {
        INST* const pinst = PinstFromPpib( ppib );
        CallR( pinst->ErrINSTCreateTempDatabase() );
        if ( pinst->m_fTempDBCreated )
        {
            DBSetOpenDatabaseFlag( ppib, pinst->m_mpdbidifmp[ dbidTemp ] );
        }
    }

    return err;
}

ERR ErrEnsureTempDatabaseOpened( _In_ const INST* const pinst, _In_ const PIB * const ppib )
{
    ERR err = JET_errSuccess;

    if ( UlParam( pinst, JET_paramMaxTemporaryTables ) <= 0 )
    {
        Error( ErrERRCheck( JET_errIllegalOperation ) );
    }

    Assert ( FPIBUserOpenedDatabase( ppib, dbidTemp ) );
    if ( !FPIBUserOpenedDatabase( ppib, dbidTemp ) )
    {
        Error( ErrERRCheck( JET_errInternalError ) );
    }

HandleError:
    return err;
}

ERR VTAPI ErrIsamSetSessionContext( JET_SESID sesid, DWORD_PTR dwContext )
{
    PIB*    ppib    = (PIB *)sesid;

    if ( dwPIBSessionContextNull == dwContext
        || dwPIBSessionContextUnusable == dwContext )
        return ErrERRCheck( JET_errInvalidParameter );

    return ppib->ErrPIBSetSessionContext( dwContext );
}

ERR VTAPI ErrIsamResetSessionContext( JET_SESID sesid )
{
    ERR     err         = JET_errSuccess;
    PIB*    ppib        = (PIB *)sesid;

    CallR( ppib->ErrPIBCheckCorrectSessionContext() );

    ppib->PIBResetSessionContext( fTrue );

    return JET_errSuccess;
}

VOID PIBReportSessionSharingViolation( const PIB * const ppib )
{
    WCHAR wszSession[32];
    WCHAR wszSesContext[cchContextStringSize];
    WCHAR wszSesContextThreadID[cchContextStringSize];
    WCHAR wszThreadID[32];
    WCHAR wszTransactionIds[512];

    OSStrCbFormatW( wszSession, sizeof(wszSession), L"0x%p", ppib );
    ppib->PIBGetSessContextInfo( wszSesContextThreadID, wszSesContext );
    OSStrCbFormatW( wszThreadID, sizeof(wszThreadID), L"0x%0*I64X", (ULONG)sizeof(DWORD_PTR)*2, QWORD( DwUtilThreadId() ) );

    (void)ppib->TrxidStack().ErrDump( wszTransactionIds, _countof( wszTransactionIds ) );

    const WCHAR *rgcwszT[] = { wszSession, wszSesContext, wszSesContextThreadID, wszThreadID, wszTransactionIds };
    UtilReportEvent(
            eventError,
            TRANSACTION_MANAGER_CATEGORY,
            SESSION_SHARING_VIOLATION_ID,
            _countof(rgcwszT),
            rgcwszT,
            0,
            NULL,
            PinstFromPpib( ppib ) );
}


ERR VTAPI ErrIsamResetCounter( JET_SESID sesid, INT CounterType )
{
    return ErrERRCheck( JET_errFeatureNotAvailable );
}

ERR VTAPI ErrIsamGetCounter( JET_SESID sesid, INT CounterType, LONG *plValue )
{
    return ErrERRCheck( JET_errFeatureNotAvailable );
}

