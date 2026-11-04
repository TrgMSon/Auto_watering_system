# Stage 1: Build stage
FROM maven:3.9.6-eclipse-temurin-21 AS build
WORKDIR /app

# Copy pom.xml và source code
COPY . .

# Build ra file JAR (bỏ qua test)
RUN mvn clean package -DskipTests

# Stage 2: Run stage (Sử dụng Alpine cho nhẹ)
FROM eclipse-temurin:21-jre-alpine
WORKDIR /app

# --- QUAN TRỌNG: Thiết lập múi giờ tại đây ---
RUN apk add --no-cache tzdata
ENV TZ=Asia/Ho_Chi_Minh

# Copy file JAR từ Stage 1 sang Stage 2
# Lưu ý: Nếu có nhiều file .jar trong target, lệnh này có thể gây lỗi. 
# Tốt nhất là chỉ định rõ hoặc dùng lệnh rename ở stage 1.
COPY --from=build /app/target/*.jar app.jar

EXPOSE 8080

# Chạy ứng dụng
ENTRYPOINT ["java", "-jar", "app.jar"]
