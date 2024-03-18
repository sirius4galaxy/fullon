from .testUtils import Utils
from .Cluster import Cluster
from .WalletMgr import WalletMgr
from datetime import datetime
import platform

import argparse

class AppArgs:
    def __init__(self):
        self.args=[]

    class AppArg:
        def __init__(self, flag, help, type=None, default=None, choices=None, action=None):
            self.flag=flag
            self.type=type
            self.help=help
            self.default=default
            self.choices=choices
            self.action=action

    def add(self, flag, type, help, default, choices=None):
        arg=self.AppArg(flag, help, type=type, default=default, choices=choices)
        self.args.append(arg)


    def add_bool(self, flag, help, action='store_true'):
        arg=self.AppArg(flag, help, action=action)
        self.args.append(arg)

# pylint: disable=too-many-instance-attributes
class TestHelper(object):
    LOCAL_HOST="localhost"
    DEFAULT_PORT=8888
    DEFAULT_WALLET_PORT=9899

    @staticmethod
    # pylint: disable=too-many-branches
    # pylint: disable=too-many-statements
    def createArgumentParser(includeArgs, applicationSpecificArgs=AppArgs()) -> argparse.ArgumentParser:
        """Accepts set of arguments, builds argument parser and returns parse_args() output."""
        assert(includeArgs)
        assert(isinstance(includeArgs, set))
        assert(isinstance(applicationSpecificArgs, AppArgs))

        thParser = argparse.ArgumentParser(add_help=False, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
        thGrpTitle = "Test Helper Arguments"
        thGrpDescription="Test Helper configuration items used to configure and spin up the regression test framework and blockchain environment."
        thGrp = thParser.add_argument_group(title=thGrpTitle, description=thGrpDescription)
        thGrp.add_argument('-?', '--help', action='help', default=argparse.SUPPRESS, help=argparse._('show this help message and exit'))

        if "-p" in includeArgs:
            thGrp.add_argument("-p", type=int, help="producing nodes count", default=1)
        if "-n" in includeArgs:
            thGrp.add_argument("-n", type=int, help="total nodes", default=0)
        if "-d" in includeArgs:
            thGrp.add_argument("-d", type=int, help="delay between nodes startup", default=1)
        if "--nodes-file" in includeArgs:
            thGrp.add_argument("--nodes-file", type=str, help="File containing nodes info in JSON format.")
        if "-s" in includeArgs:
            thGrp.add_argument("-s", type=str, help="topology", choices=["mesh"], default="mesh")
        if "-c" in includeArgs:
            thGrp.add_argument("-c", type=str, help="chain strategy",
                    choices=[Utils.SyncResyncTag, Utils.SyncReplayTag, Utils.SyncNoneTag, Utils.SyncHardReplayTag],
                    default=Utils.SyncResyncTag)
        if "--kill-sig" in includeArgs:
            thGrp.add_argument("--kill-sig", type=str, choices=[Utils.SigKillTag, Utils.SigTermTag], help="kill signal.",
                    default=Utils.SigKillTag)
        if "--kill-count" in includeArgs:
            thGrp.add_argument("--kill-count", type=int, help="fonod instances to kill", default=-1)
        if "--terminate-at-block" in includeArgs:
            thGrp.add_argument("--terminate-at-block", type=int, help="block to terminate on when replaying", default=0)
        if "--seed" in includeArgs:
            thGrp.add_argument("--seed", type=int, help="random seed", default=1)

        if "--host" in includeArgs:
            thGrp.add_argument("-h", "--host", type=str, help="%s host name" % (Utils.EosServerName),
                                     default=TestHelper.LOCAL_HOST)
        if "--port" in includeArgs:
            thGrp.add_argument("--port", type=int, help="%s host port" % Utils.EosServerName,
                                     default=TestHelper.DEFAULT_PORT)
        if "--wallet-host" in includeArgs:
            thGrp.add_argument("--wallet-host", type=str, help="%s host" % Utils.EosWalletName,
                                     default=TestHelper.LOCAL_HOST)
        if "--wallet-port" in includeArgs:
            thGrp.add_argument("--wallet-port", type=int, help="%s port" % Utils.EosWalletName,
                                     default=TestHelper.DEFAULT_WALLET_PORT)
        if "--prod-count" in includeArgs:
            thGrp.add_argument("-c", "--prod-count", type=int, help="Per node producer count", default=1)
        if "--defproducera_prvt_key" in includeArgs:
            thGrp.add_argument("--defproducera_prvt_key", type=str, help="defproducera private key.")
        if "--defproducerb_prvt_key" in includeArgs:
            thGrp.add_argument("--defproducerb_prvt_key", type=str, help="defproducerb private key.")
        if "--dump-error-details" in includeArgs:
            thGrp.add_argument("--dump-error-details",
                                     help="Upon error print etc/eosio/node_*/config.ini and <test_name><pid>/node_*/stderr.log to stdout",
                                     action='store_true')
        if "--dont-launch" in includeArgs:
            thGrp.add_argument("--dont-launch", help="Don't launch own node. Assume node is already running.",
                                     action='store_true')
        if "--keep-logs" in includeArgs:
            thGrp.add_argument("--keep-logs", help="Don't delete <test_name><pid>/node_* folders, or other test specific log directories, upon test completion",
                                     action='store_true')
        if "-v" in includeArgs:
            thGrp.add_argument("-v", help="verbose logging", action='store_true')
        if "--leave-running" in includeArgs:
            thGrp.add_argument("--leave-running", help="Leave cluster running after test finishes", action='store_true')
        if "--only-bios" in includeArgs:
            thGrp.add_argument("--only-bios", help="Limit testing to bios node.", action='store_true')
        if "--clean-run" in includeArgs:
            thGrp.add_argument("--clean-run", help="Kill all fonod and gaxkey instances", action='store_true')
        if "--sanity-test" in includeArgs:
            thGrp.add_argument("--sanity-test", help="Validates fonod and gaxkey are in path and can be started up.", action='store_true')
        if "--alternate-version-labels-file" in includeArgs:
            thGrp.add_argument("--alternate-version-labels-file", type=str, help="Provide a file to define the labels that can be used in the test and the path to the version installation associated with that.")
        if "--error-log-path" in includeArgs:
            thGrp.add_argument("--error-log-path", type=str, help="Provide path to error file for use when remotely running a test from another test.")
        if "--unshared" in includeArgs:
            thGrp.add_argument("--unshared", help="Run test in isolated network namespace", action='store_true')

        if len(applicationSpecificArgs.args) > 0:
            appArgsGrpTitle="Application Specific Arguments"
            appArgsGrpdescription="Test Helper configuration items used to configure and spin up the regression test framework and blockchain environment."
            appArgsGrp = thParser.add_argument_group(title=appArgsGrpTitle, description=appArgsGrpdescription)
            for arg in applicationSpecificArgs.args:
                if arg.type is not None:
                    appArgsGrp.add_argument(arg.flag, type=arg.type, help=arg.help, choices=arg.choices, default=arg.default)
                else:
                    appArgsGrp.add_argument(arg.flag, help=arg.help, action=arg.action)

        return thParser

    @staticmethod
    # pylint: disable=too-many-branches
    # pylint: disable=too-many-statements
    def parse_args(includeArgs, applicationSpecificArgs=AppArgs()):
        parser = TestHelper.createArgumentParser(includeArgs=includeArgs, applicationSpecificArgs=applicationSpecificArgs)
        args = parser.parse_args()
        return args

    @staticmethod
    def printSystemInfo(prefix):
        """Print system information to stdout. Print prefix first."""
        if prefix:
            Utils.Print(str(prefix))
        clientVersion=Cluster.getClientVersion()
        Utils.Print("UTC time: %s" % str(datetime.utcnow()))
        Utils.Print("EOS Client version: %s" % (clientVersion))
        Utils.Print("Processor: %s" % (platform.processor()))
        Utils.Print("OS name: %s" % (platform.platform()))

    @staticmethod
    # pylint: disable=too-many-arguments
    def shutdown(cluster, walletMgr, testSuccessful=True, killEosInstances=True, killWallet=True, keepLogs=False, cleanRun=True, dumpErrorDetails=False):
        """Cluster and WalletMgr shutdown and cleanup."""
        assert(cluster)
        assert(isinstance(cluster, Cluster))
        if walletMgr:
            assert(isinstance(walletMgr, WalletMgr))
        assert(isinstance(testSuccessful, bool))
        assert(isinstance(killEosInstances, bool))
        assert(isinstance(killWallet, bool))
        assert(isinstance(cleanRun, bool))
        assert(isinstance(dumpErrorDetails, bool))

        Utils.ShuttingDown=True

        if testSuccessful:
            Utils.Print("Test succeeded.")
        else:
            Utils.Print("Test failed.")

        def reportProductionAnalysis(thresholdMs):
            Utils.Print(Utils.FileDivider)
            for node in cluster.getAllNodes():
                missedBlocks=node.analyzeProduction(thresholdMs=thresholdMs)
                if len(missedBlocks) > 0:
                    Utils.Print("NodeId: %s produced the following blocks late: %s" % (node.nodeId, missedBlocks))

        if not testSuccessful and dumpErrorDetails:
            cluster.reportStatus()
            Utils.Print(Utils.FileDivider)
            psOut = Cluster.pgrepEosServers(timeout=60)
            Utils.Print("pgrep output:\n%s" % (psOut))
            reportProductionAnalysis(thresholdMs=0)
            Utils.Print("== Errors see above ==")
        elif dumpErrorDetails:
            # for now report these to know how many blocks we are missing production windows for
            reportProductionAnalysis(thresholdMs=200)

        if killEosInstances:
            Utils.Print("Shut down the cluster.")
            cluster.killall(allInstances=cleanRun, kill=testSuccessful)
            if testSuccessful and not keepLogs:
                Utils.Print("Cleanup cluster data.")
                cluster.cleanup()

        if walletMgr and killWallet:
            Utils.Print("Shut down the wallet.")
            walletMgr.killall(allInstances=cleanRun)
            if testSuccessful and not keepLogs:
                Utils.Print("Cleanup wallet data.")
                walletMgr.cleanup()

