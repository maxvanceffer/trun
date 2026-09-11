#include <QTest>
#include <QSignalSpy>
#include <csignal>
#include <unistd.h>
#ifndef Q_OS_WINDOWS
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include "commandexecutor.h"

class TestCommandExecutor : public QObject {
    Q_OBJECT

private slots:
    void testRunEcho();
    void testStop();
    void testBadCommand();
    void testKillAll();
    void testDetachAllLeavesProcessRunning();
    void testRunningCommandIdsTracked();
    void testStopCommandById();
    void testAdoptExternalRoundTrip();
    void testRunningProcessesSnapshot();
    void testProcessStats();
    void testRevealFolderValidation();
    void testElapsedCpuAndTotalMemory();
    void testAdoptRestoresStartTime();
    void testPortBusyFreeAndOccupied();
    void testKillExternalGuards();
};

void TestCommandExecutor::testRunEcho() {
    CommandExecutor executor;
    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);
    QSignalSpy outSpy(&executor, &CommandExecutor::outputReceived);

    const int id = executor.run("/bin/echo", {"hello-executor"}, "", "echo-test", "test:echo");
    QVERIFY2(id > 0, "run() must return a positive id");
    QVERIFY(executor.isRunning(id));

    QVERIFY2(finishedSpy.wait(5000), "finished() never arrived");
    QVERIFY(!executor.isRunning(id));

    QCOMPARE(finishedSpy.size(), 1);
    QCOMPARE(finishedSpy.first().at(0).toInt(), id);
    QCOMPARE(finishedSpy.first().at(2).toInt(), 0);

    bool sawLine = false;
    for (const auto &args : outSpy) {
        if (args.at(0).toInt() == id && !args.at(2).toBool()
            && args.at(3).toString() == "hello-executor") {
            sawLine = true;
        }
    }
    QVERIFY2(sawLine, "stdout line 'hello-executor' never arrived");
}

void TestCommandExecutor::testStop() {
    CommandExecutor executor;
    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);

    const int id = executor.run("/bin/sleep", {"10"}, "", "sleep-test", "test:sleep");
    QVERIFY(id > 0);
    QVERIFY(executor.isRunning(id));

    executor.stop(id);
    QVERIFY2(finishedSpy.wait(5000), "finished() never arrived after stop()");
    QVERIFY(!executor.isRunning(id));
    QCOMPARE(finishedSpy.first().at(0).toInt(), id);
}

void TestCommandExecutor::testBadCommand() {
    CommandExecutor executor;
    QSignalSpy failedSpy(&executor, &CommandExecutor::failed);

    const int id = executor.run("/nonexistent-trun-binary-xyz", {}, "", "bad-test", "test:bad");
    QVERIFY(id > 0);
    QVERIFY2(failedSpy.wait(5000), "failed() never arrived for bad command");
    QVERIFY(!executor.isRunning(id));
    QCOMPARE(failedSpy.first().at(0).toInt(), id);
}

void TestCommandExecutor::testKillAll() {
    CommandExecutor executor;
    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);

    const int first = executor.run("/bin/sleep", {"10"}, "", "sleep-a", "test:sleep-a");
    const int second = executor.run("/bin/sleep", {"10"}, "", "sleep-b", "test:sleep-b");
    QVERIFY(first > 0 && second > 0 && first != second);
    QCOMPARE(executor.runningCount(), 2);

    executor.killAll();
    QVERIFY2(finishedSpy.wait(5000), "finished() never arrived after killAll()");
    QTest::qWait(200);
    QCOMPARE(executor.runningCount(), 0);
    QCOMPARE(finishedSpy.size(), 2);
}

void TestCommandExecutor::testDetachAllLeavesProcessRunning() {
    CommandExecutor executor;
    QSignalSpy startedSpy(&executor, &CommandExecutor::started);

    const int id = executor.run("/bin/sleep", {"30"}, "", "sleep-detach", "test:sleep-detach");
    QVERIFY(id > 0);
    QVERIFY2(startedSpy.wait(5000), "started() never arrived");
    const int pid = startedSpy.first().at(1).toInt();
    QVERIFY(pid > 0);

    executor.detachAll();
    QCOMPARE(executor.runningCount(), 0);
    QVERIFY(!executor.isRunning(id));

    // The child must survive the detach: process still exists
    QVERIFY2(::kill(static_cast<pid_t>(pid), 0) == 0, "detached process died");
    ::kill(static_cast<pid_t>(pid), SIGKILL);
}

void TestCommandExecutor::testRunningCommandIdsTracked() {
    CommandExecutor executor;
    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);

    QVERIFY(!executor.runningCommandIds().contains("test:tracked"));
    const int id = executor.run("/bin/sleep", {"10"}, "", "tracked", "test:tracked");
    QVERIFY(id > 0);
    QVERIFY(executor.runningCommandIds().contains("test:tracked"));

    executor.stop(id);
    QVERIFY(finishedSpy.wait(5000));
    QVERIFY(!executor.runningCommandIds().contains("test:tracked"));
}

void TestCommandExecutor::testStopCommandById() {
    CommandExecutor executor;
    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);

    const int id = executor.run("/bin/sleep", {"10"}, "", "victim", "test:victim");
    QVERIFY(id > 0);
    executor.stopCommand("test:victim");
    QVERIFY2(finishedSpy.wait(5000), "finished() never arrived after stopCommand()");
    QVERIFY(!executor.runningCommandIds().contains("test:victim"));

    // Unknown id is a no-op, must not crash or emit
    executor.stopCommand("test:nope");
    QCOMPARE(finishedSpy.size(), 1);
}

void TestCommandExecutor::testAdoptExternalRoundTrip() {
    CommandExecutor owner;
    QSignalSpy startedSpy(&owner, &CommandExecutor::started);
    const int id = owner.run("/bin/sleep", {"30"}, "", "adoptee", "test:adoptee");
    QVERIFY(id > 0);
    QVERIFY(startedSpy.wait(5000));
    const int pid = startedSpy.first().at(1).toInt();
    QVERIFY(pid > 0);

    CommandExecutor adopted;
    QVERIFY(!adopted.adoptExternal(pid, "/bin/false", {"30"}, "", "wrong", "test:wrong"));
    QVERIFY(adopted.adoptExternal(pid, "/bin/sleep", {"30"}, "", "adoptee", "test:adoptee"));
    QVERIFY(adopted.runningCommandIds().contains("test:adoptee"));

    // Stop by command id kills the adopted OS process.
    // Note: the emission is synchronous, so assert count() directly —
    // QSignalSpy::wait() only observes emissions that happen during the wait.
    QSignalSpy finishedSpy(&adopted, &CommandExecutor::finished);
    adopted.stopCommand("test:adoptee");
    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(!adopted.runningCommandIds().contains("test:adoptee"));
    // Give the owner's QProcess a chance to reap the killed child
    // (a zombie pid still answers kill(pid, 0))
    QTest::qWait(300);
    QVERIFY(::kill(static_cast<pid_t>(pid), 0) != 0);

    owner.detachAll();
}

void TestCommandExecutor::testRunningProcessesSnapshot() {
    CommandExecutor executor;
    QSignalSpy startedSpy(&executor, &CommandExecutor::started);
    const int id = executor.run("/bin/sleep", {"30"}, "", "snap", "test:snap");
    QVERIFY(id > 0);
    QVERIFY(startedSpy.wait(5000));

    const QVariantList snap = executor.runningProcesses();
    QCOMPARE(snap.size(), 1);
    const QVariantMap m = snap.first().toMap();
    QVERIFY(m.value("pid").toInt() > 0);
    QCOMPARE(m.value("commandId").toString(), QString("test:snap"));
    QCOMPARE(m.value("label").toString(), QString("snap"));

    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);
    executor.killAll();
    QVERIFY(finishedSpy.wait(5000));
}

void TestCommandExecutor::testProcessStats() {
    CommandExecutor executor;
    QSignalSpy finishedSpy(&executor, &CommandExecutor::finished);

    QCOMPARE(executor.pidForCommand("test:stats"), 0);
    QCOMPARE(executor.memoryForCommand("test:stats"), -1);

    const int id = executor.run("/bin/sleep", {"30"}, "", "stats", "test:stats");
    QVERIFY(id > 0);

    QSignalSpy startedSpy(&executor, &CommandExecutor::started);
    QVERIFY(startedSpy.wait(5000));
    const int pid = executor.pidForCommand("test:stats");
    QVERIFY(pid > 0);
    QVERIFY(executor.memoryForCommand("test:stats") >= 0);

    executor.stop(id);
    QVERIFY(finishedSpy.wait(5000));
    QCOMPARE(executor.pidForCommand("test:stats"), 0);
    QCOMPARE(executor.memoryForCommand("test:stats"), -1);
}

void TestCommandExecutor::testElapsedCpuAndTotalMemory() {
#ifndef Q_OS_WINDOWS
    CommandExecutor executor;
    QSignalSpy startedSpy(&executor, &CommandExecutor::started);

    QCOMPARE(executor.elapsedForCommand("test:uptime"), -1);
    QCOMPARE(executor.cpuForCommand("test:uptime"), -1.0);

    const int id = executor.run("/bin/sleep", {"30"}, "", "uptime", "test:uptime");
    QVERIFY(id > 0);
    QVERIFY(startedSpy.wait(5000));

    const qlonglong elapsed = executor.elapsedForCommand("test:uptime");
    QVERIFY2(elapsed >= 0 && elapsed < 30, qPrintable(QString("elapsed=%1").arg(elapsed)));
    QVERIFY(executor.cpuForCommand("test:uptime") >= 0.0);
    QVERIFY(executor.totalMemoryKb() > 0);

    executor.stop(id);
#else
    QSKIP("stats require ps/sysctl (non-Windows)");
#endif
}

void TestCommandExecutor::testAdoptRestoresStartTime() {
#ifndef Q_OS_WINDOWS
    CommandExecutor owner;
    QSignalSpy startedSpy(&owner, &CommandExecutor::started);
    const int id = owner.run("/bin/sleep", {"30"}, "", "st", "test:st");
    QVERIFY(id > 0);
    QVERIFY(startedSpy.wait(5000));
    const int pid = startedSpy.first().at(1).toInt();
    QVERIFY(pid > 0);

    // Persist like the app does on exit with "leave running"
    const QVariantList snap = owner.runningProcesses();
    QCOMPARE(snap.size(), 1);
    QVERIFY(!snap.first().toMap().value("startedAt").toString().isEmpty());

    // Fresh instance, like an app restart: uptime continues from saved start
    CommandExecutor adopted;
    const QVariantMap m = snap.first().toMap();
    QVERIFY(adopted.adoptExternal(pid, m["command"].toString(), m["args"].toStringList(),
                                  m["workingDir"].toString(), m["label"].toString(),
                                  m["commandId"].toString(), m["startedAt"].toString()));
    const qlonglong elapsed = adopted.elapsedForCommand("test:st");
    QVERIFY2(elapsed >= 0 && elapsed < 30, qPrintable(QString("elapsed=%1").arg(elapsed)));

    adopted.stopCommand("test:st");
    owner.detachAll();
#else
    QSKIP("adopt requires ps (non-Windows)");
#endif
}

void TestCommandExecutor::testPortBusyFreeAndOccupied() {
#ifndef Q_OS_WINDOWS
    CommandExecutor executor;
    QVERIFY(!executor.portBusy(0));
    QVERIFY(!executor.portBusy(-1));
    QVERIFY(!executor.portBusy(99999));
    QCOMPARE(executor.pidOnPort(0), 0);

    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    QVERIFY(fd >= 0);
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = 0; // ephemeral
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    QVERIFY(::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);
    QVERIFY(::listen(fd, 1) == 0);
    socklen_t len = sizeof(addr);
    QVERIFY(::getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0);
    const int port = ntohs(addr.sin_port);
    QVERIFY(port > 0);

    QVERIFY(executor.portBusy(port));
    QCOMPARE(executor.pidOnPort(port), static_cast<int>(::getpid()));
    ::close(fd);
    QVERIFY(!executor.portBusy(port));
#else
    QSKIP("port checks are POSIX-only");
#endif
}

void TestCommandExecutor::testKillExternalGuards() {
#ifndef Q_OS_WINDOWS
    CommandExecutor executor;
    QVERIFY(!executor.killExternal(1)); // never init
    QVERIFY(!executor.killExternal(static_cast<int>(::getpid()))); // never self
    QVERIFY(!executor.killExternal(0));
    QVERIFY(!executor.killExternal(1 << 22)); // ESRCH
#else
    QSKIP("port checks are POSIX-only");
#endif
}

void TestCommandExecutor::testRevealFolderValidation() {
    CommandExecutor executor;
    QVERIFY(!executor.revealFolder(""));
    QVERIFY(!executor.revealFolder("/nonexistent-trun-dir-xyz"));
}

QTEST_MAIN(TestCommandExecutor)
#include "test_commandexecutor.moc"
