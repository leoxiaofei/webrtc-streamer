#!/bin/bash
# ---------------------------------------------------------------------------
# 一键编译 + 推送 webrtc-streamer 到部署机并重启服务
#
# 用法:
#   ./deploy.sh              # 编译 + 推送二进制和前端 + 重启服务
#   ./deploy.sh --log        # 同上，服务以 -vvv 详细日志启动（RTSP 收发全 dump）
#   ./deploy.sh --no-restart # 编译 + 推送，不重启（下次启动生效）
#   ./deploy.sh --bin-only   # 只编译 + 推送二进制（前端不动）+ 重启
# ---------------------------------------------------------------------------
set -e

TARGET=leoxiaofei@192.168.183.134
DEPLOY_DIR=/WsCode/webrtc-streamer-deploy
PORT=8000
LOGLEVEL=""
RESTART=1
PUSH_HTML=1
for arg in "$@"; do
	case "$arg" in
		--log)       LOGLEVEL="-vvv" ;;
		--no-restart) RESTART=0 ;;
		--bin-only)  PUSH_HTML=0 ;;
		*) echo "unknown option: $arg"; exit 1 ;;
	esac
done

cd "$(dirname "$0")"

echo "==> 编译"
make webrtc-streamer -j"$(nproc)" 2>&1 | tee /tmp/webrtc-streamer-build.log | grep -E " error|Linking CXX executable" || true
if [ "${PIPESTATUS[0]}" -ne 0 ]; then
	echo "编译失败，详见 /tmp/webrtc-streamer-build.log"
	exit 1
fi

echo "==> 推送二进制"
scp -q webrtc-streamer "$TARGET:$DEPLOY_DIR/webrtc-streamer.new"

if [ "$PUSH_HTML" -eq 1 ]; then
	echo "==> 推送前端"
	scp -q html/*.js html/*.html html/styles.css "$TARGET:$DEPLOY_DIR/html/"
fi

if [ "$RESTART" -eq 1 ]; then
	echo "==> 重启服务 (端口 $PORT${LOGLEVEL:+，详细日志})"
	ssh -n "$TARGET" "P=\$(pgrep -x webrtc-streamer | head -1); [ -n \"\$P\" ] && kill \$P; sleep 1; \
		cd $DEPLOY_DIR && mv -f webrtc-streamer.new webrtc-streamer && chmod +x webrtc-streamer && \
		(setsid ./webrtc-streamer -H0.0.0.0:$PORT $LOGLEVEL > $DEPLOY_DIR/webrtc-streamer.log 2>&1 < /dev/null &)"
else
	ssh -n "$TARGET" "cd $DEPLOY_DIR && mv -f webrtc-streamer.new webrtc-streamer && chmod +x webrtc-streamer"
fi

echo "==> 验证"
sleep 3
VERSION=$(curl -s -m 5 "http://$TARGET:$PORT/api/version" | head -c 40)
if [ -n "$VERSION" ]; then
	echo "部署成功: $VERSION"
	if [ -n "$LOGLEVEL" ]; then
		echo "日志: ssh $TARGET tail -f $DEPLOY_DIR/webrtc-streamer.log"
	fi
else
	echo "警告: 服务未响应，请检查 ssh $TARGET 'tail -20 $DEPLOY_DIR/webrtc-streamer.log'"
	exit 1
fi
exit 0
