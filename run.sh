az group create --name sdc --location eastus
az acr create -n sdcrepo -g sdc --sku basic
az acr login --name sdcrepo
az acr list --resource-group sdc --query "[].{acrLoginServer:loginServer}" --output table

docker build -f docker/Dockerfile.worker -t sdcrepo.azurecr.io/worker .
docker build -f docker/Dockerfile.master -t sdcrepo.azurecr.io/master .

docker push sdcrepo.azurecr.io/master
docker push sdcrepo.azurecr.io/worker

az aks create -n sdccluster -g sdc --generate-ssh-keys --attach-acr sdcrepo --node-count 7 --enable-addons monitoring
az aks get-credentials -g sdc -n sdccluster

# helm repo add bitnami https://charts.bitnami.com/bitnami
helm install sdc bitnami/zookeeper

kubectl apply -f yaml/worker-crash.yaml
kubectl apply -f yaml/worker-deployment.yaml
kubectl apply -f yaml/worker-service.yaml

kubectl apply -f yaml/master-crash.yaml
kubectl apply -f yaml/master-deployment.yaml
kubectl apply -f yaml/master-service.yaml
kubectl get all

curl -X POST 52.226.231.94:18080/200/400/3/sdc
curl -X POST 52.226.231.94:18080/20/5/25m

kubectl delete --all deployments
kubectl delete deployment master-deployment
kubectl delete deployment master-crash
kubectl delete deployment worker-deployment
kubectl delete deployment worker-crash

az group delete --name sdc --yes --no-wait
kubectl logs --follow pod/
