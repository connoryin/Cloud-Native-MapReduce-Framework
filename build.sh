# Script to build your protobuf, c++ binaries, and docker images here
cmake .
make

kind create cluster

kubectl create ns sdc

#helm repo add bitnami https://charts.bitnami.com/bitnami
#helm install -n sdc sdc bitnami/zookeeper

docker build -f docker/Dockerfile.worker -t sdcrepo.azurecr.io/worker .
docker build -f docker/Dockerfile.master -t sdcrepo.azurecr.io/master .

# docker build -f docker/Dockerfile.worker -t worker .
# docker build -f docker/Dockerfile.master -t master .

# kind load docker-image worker
# kind load docker-image master

#kubectl -n sdc apply -f yaml/worker-deployment.yaml
#kubectl -n sdc apply -f yaml/worker-service.yaml
#
#kubectl -n sdc apply -f yaml/master-deployment.yaml
#kubectl -n sdc apply -f yaml/master-service.yaml

kubectl apply -f yaml/worker-deployment.yaml
kubectl apply -f yaml/worker-service.yaml

kubectl apply -f yaml/master-deployment.yaml
kubectl apply -f yaml/master-service.yaml

#linkerd install | kubectl apply -f -
#linkerd check
#kubectl get -n sdc deploy -o yaml \
#  | linkerd inject - \
#  | kubectl apply -f -
#
#linkerd -n sdc check --proxy

kubectl get all -n sdc
#kubectl -n sdc logs pod/<your-pod-id>
#kubectl -n sdc delete pod/<your-pod-id>
