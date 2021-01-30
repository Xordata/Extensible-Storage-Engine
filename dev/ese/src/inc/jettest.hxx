// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#ifndef JETTEST_HXX_INCLUDED
#define JETTEST_HXX_INCLUDED

#ifdef ENABLE_JET_UNIT_TEST

class JetUnitTestFailure
{
    public:
        JetUnitTestFailure(
            const char * const szFile,
            const INT          line,
            const char * const szCondition );
        ~JetUnitTestFailure();

        const char * SzFile() const;
        const char * SzCondition() const;
        INT Line() const;

    private:
        const char * const m_szFile;
        const char * const m_szCondition;
        const INT m_line;

    private:
        JetUnitTestFailure();
        JetUnitTestFailure( const JetUnitTestFailure& );
        JetUnitTestFailure& operator=( const JetUnitTestFailure& );
};

class JetUnitTestResult
{
    public:
        JetUnitTestResult();
        ~JetUnitTestResult();

        void Start( const char * const szTest );
        void Finish();
        void AddFailure( const JetUnitTestFailure& failure );

        INT Failures() const;

    private:
        const char * m_szTest;
        INT m_failures;

    private:
        JetUnitTestResult( const JetUnitTestResult& );
        JetUnitTestResult& operator=( const JetUnitTestResult& );
};

class JetUnitTest
{
    public:
        static INT RunTests( const char * const szTest, const IFMP ifmpTest );
        static void PrintTests();

    private:
        static JetUnitTest * s_ptestHead;
        
    public:
        static const char chWildCard = '*';

    protected:
        JetUnitTest( const char * const szName );
        virtual ~JetUnitTest();

        virtual bool FNeedsBF();
        virtual bool FNeedsDB();
        virtual bool FRunByDefault();
        virtual void SetTestIfmp( const IFMP ifmpTest );

        virtual void Run( JetUnitTestResult * const presult ) = 0;
        
    private:
        JetUnitTest * m_ptestNext;
        const char * const m_szName;
};

class JetSimpleUnitTest : public JetUnitTest
{
    public:
        static const DWORD dwBufferManager = 0x1;
        static const DWORD dwOpenDatabase = 0x2;
        static const DWORD dwDontRunByDefault = 0x4;

    protected:
        JetSimpleUnitTest( const char * const szName );
        JetSimpleUnitTest( const char * const szName, const DWORD dwFacilities );
        virtual ~JetSimpleUnitTest();

        bool FNeedsBF() { return m_dwFacilities & dwBufferManager; }
        bool FNeedsDB() { return false; }
        bool FRunByDefault() { return !(m_dwFacilities & dwDontRunByDefault); }

        void Run( JetUnitTestResult * const presult );

        virtual void Run_() = 0;
        void Fail_( const char * const szFile, const INT line, const char * const szCondition );

    private:
        DWORD               m_dwFacilities;

    protected:
        JetUnitTestResult * m_presult;
};

template<class FIXTURE>
class JetTestCaller : public JetSimpleUnitTest
{
public:
    typedef void (FIXTURE::*PfnTestMethod)();
    JetTestCaller( const char * const szName, PfnTestMethod pfnTest );
    JetTestCaller( const char * const szName, const DWORD dwFacilities, PfnTestMethod pfnTest );

protected:
    void Run_();

private:
    const PfnTestMethod m_pfnTest;
    const char * const m_szName;
};

template<class FIXTURE>
JetTestCaller<FIXTURE>::JetTestCaller( const char * const szName, PfnTestMethod pfnTest ) :
    JetSimpleUnitTest( szName ),
    m_pfnTest( pfnTest ),
    m_szName( szName )
{
}

template<class FIXTURE>
JetTestCaller<FIXTURE>::JetTestCaller( const char * const szName, const DWORD dwFacilities, PfnTestMethod pfnTest ) :
    JetSimpleUnitTest( szName, dwFacilities ),
    m_pfnTest( pfnTest ),
    m_szName( szName )
{
}

template<class FIXTURE>
void JetTestCaller<FIXTURE>::Run_()
{
    FIXTURE fixture;
    if( fixture.SetUp(m_presult) )
    {
        (fixture.*m_pfnTest)();
        fixture.TearDown();
    }
    else
    {
        JetUnitTestFailure failure( __FILE__, __LINE__, "Test fixture setup failed" );
        m_presult->AddFailure( failure );
    }
}

class JetSimpleDbUnitTest : public JetSimpleUnitTest
{
    protected:
        JetSimpleDbUnitTest( const char * const szName );
        JetSimpleDbUnitTest( const char * const szName, const DWORD dwFacilities );
        virtual ~JetSimpleDbUnitTest();

        bool FNeedsDB() { return dwOpenDatabase == ( m_dwFacilities & dwOpenDatabase ); }
        void SetTestIfmp( const IFMP ifmpTest );
        IFMP IfmpTest();

        void Run( JetUnitTestResult * const presult );

        virtual void Run_() = 0;
        void Fail_( const char * const szFile, const INT line, const char * const szCondition );

    private:
        IFMP                m_ifmp;
        DWORD               m_dwFacilities;
        JetUnitTestResult * m_presult;
};

class JetTestFixture
{
    public:
        bool SetUp( JetUnitTestResult * const presult );
        void TearDown();
        
    protected:
        JetTestFixture();
        virtual ~JetTestFixture();

        void Fail_( const char * const szFile, const INT line, const char * const szCondition );

        virtual bool SetUp_() = 0;
        virtual void TearDown_() = 0;

    private:
        JetUnitTestResult * m_presult;
};


#define JETUNITTEST(component,test) \
class Test##component##test : public JetSimpleUnitTest                  \
{                                                                       \
protected:                                                              \
    void Run_();                                                        \
private:                                                                \
    Test##component##test() : JetSimpleUnitTest(#component "." #test) {}\
    static Test##component##test s_instance;                            \
};                                                                      \
Test##component##test Test##component##test::s_instance;                \
void Test##component##test::Run_()

#define JETUNITTESTEX(component,test,facilities) \
class Test##component##test : public JetSimpleUnitTest                  \
{                                                                       \
protected:                                                              \
    void Run_();                                                        \
private:                                                                \
    Test##component##test() : JetSimpleUnitTest(#component "." #test, facilities) {}\
    static Test##component##test s_instance;                            \
};                                                                      \
Test##component##test Test##component##test::s_instance;                \
void Test##component##test::Run_()



#define JETUNITTESTDB(component,test,facilities) \
class Test##component##test : public JetSimpleDbUnitTest                \
{                                                                       \
protected:                                                              \
    void Run_();                                                        \
private:                                                                \
    Test##component##test() : JetSimpleDbUnitTest(#component "." #test, facilities) {}\
    static Test##component##test s_instance;                            \
};                                                                      \
Test##component##test Test##component##test::s_instance;                \
void Test##component##test::Run_()


#define FAIL(_reason)          \
    Fail_(__FILE__, __LINE__, _reason)

#define CHECK(_condition)       \
    if (!(_condition))          \
    {                           \
        FAIL(#_condition);      \
    }

#define CHECKCALLS( _err )          \
    if ( _err != JET_errSuccess )   \
    {                               \
        wprintf( L"Last throw: %hs : %d err = %d\n", PefLastThrow()->SzFile(), PefLastThrow()->UlLine(), PefLastThrow()->Err() );   \
        FAIL( #_err " Failed with above throw site." );     \
    }

#else

#define FAIL(_reason)
#define CHECK(_condition)
#define CHECKCALLS( _err )

#define JETUNITTEST(component,test)         VOID DisabledTest##component##test( VOID )
#define JETUNITTESTEX(component,test,facilities)    VOID DisabledTest##component##test( VOID )
#define JETUNITTESTDB(component,test,facilities)    VOID DisabledTest##component##test( const IFMP ifmpTest, const WCHAR * const wszTable, JET_SESID * const psesid, JET_DBID * const pdbid, JET_TABLEID * pcursor )
#define IfmpTest()                  ((IFMP)ifmpNil)

#define CHECK( expr )

#endif

#endif

